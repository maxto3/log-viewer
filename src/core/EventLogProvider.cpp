#include "core/EventLogProvider.h"

#include "core/LogLevel.h"
#include "core/formats/WindowsFormats.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace lv {
namespace {

QString unsupportedText()
{
    return QCoreApplication::translate("EventLogBackend",
                                       "Windows event logs are only available on Windows.");
}

} // namespace

std::shared_ptr<EventLogProvider> EventLogProvider::openChannel(const EventLogBackendPtr &backend,
                                                                const QString &channel,
                                                                int maxRecords, QString *error,
                                                                EventLogError *errorKind)
{
    if (error)
        error->clear();
    if (errorKind)
        *errorKind = EventLogError::None;

    if (!backend || !backend->isSupported()) {
        if (error)
            *error = unsupportedText();
        return nullptr;
    }

    const EventLogReadResult result = backend->readChannel(channel, maxRecords);
    if (!result.error.isEmpty()) {
        if (error)
            *error = result.error;
        if (errorKind)
            *errorKind = result.errorKind;
        return nullptr;
    }

    return fromResult(channel, channel, QString(), maxRecords, result);
}

std::shared_ptr<EventLogProvider> EventLogProvider::openFile(const EventLogBackendPtr &backend,
                                                             const QString &path, int maxRecords,
                                                             QString *error,
                                                             EventLogError *errorKind)
{
    if (error)
        error->clear();
    if (errorKind)
        *errorKind = EventLogError::None;

    if (!backend || !backend->isSupported()) {
        if (error)
            *error = unsupportedText();
        return nullptr;
    }

    const EventLogReadResult result = backend->readFile(path, maxRecords);
    if (!result.error.isEmpty()) {
        if (error)
            *error = result.error;
        if (errorKind)
            *errorKind = result.errorKind;
        return nullptr;
    }

    const QFileInfo info(path);
    return fromResult(QString(), info.fileName(), info.absoluteFilePath(), maxRecords, result);
}

std::shared_ptr<EventLogProvider> EventLogProvider::fromResult(const QString &channel,
                                                               const QString &fileName,
                                                               const QString &filePath,
                                                               int maxRecords,
                                                               const EventLogReadResult &result)
{
    auto provider = std::shared_ptr<EventLogProvider>(new EventLogProvider);
    provider->load(channel, fileName, filePath, maxRecords, result);
    return provider;
}

void EventLogProvider::load(const QString &channel, const QString &fileName,
                            const QString &filePath, int maxRecords,
                            const EventLogReadResult &result)
{
    m_channel = channel;
    m_sourceName = fileName;
    m_requestedLimit = maxRecords;
    m_newestRecordId = 0;
    m_oldestRecordId = 0;

    m_entries.clear();
    m_entries.reserve(static_cast<size_t>(result.records.size()));

    for (int index = 0; index < result.records.size(); ++index) {
        const EventLogRecord &record = result.records.at(index);
        m_entries.push_back(makeEntry(record, index + 1));
        if (record.recordId > m_newestRecordId)
            m_newestRecordId = record.recordId;
        if (record.recordId > 0
            && (m_oldestRecordId == 0 || record.recordId < m_oldestRecordId))
            m_oldestRecordId = record.recordId;
    }

    m_info = DocumentInfo();
    m_info.filePath = filePath;
    m_info.fileName = fileName;
    m_info.formatId = QStringLiteral("wevt_native");
    m_info.lineCount = static_cast<int>(m_entries.size());
    m_info.truncated = result.truncated;
    m_info.mergeContinuations = false;

    m_levelCounts.clear();
    m_countsComputed = false;
}

LogEntry EventLogProvider::makeEntry(const EventLogRecord &record, qint64 fallbackLine)
{
    LogEntry entry;
    const bool parsed = WindowsEventXmlFormat::parseEventXml(record.xml, entry);
    if (!parsed)
        entry.message = record.xml;
    // EvtRender reports SystemTime in UTC; show the local wall clock time
    // like Event Viewer does (REQ-EVT-06). The absolute instant is kept, so
    // time range filters still compare correctly.
    if (entry.time.isValid())
        entry.time = entry.time.toLocalTime();

    // EvtRender does not include the rendered message: EvtFormatMessage is
    // the authority when available, the EventData fallback of parseEventXml
    // stays in place otherwise (REQ-EVT-07).
    if (!record.message.isEmpty())
        entry.message = record.message.trimmed();

    entry.sourceIndex = 0;
    // Stable, unique anchor for the first column and the view's row identity
    // (REQ-EVT-06); fall back to the ordinal only when the XML had no ID.
    entry.firstLine = record.recordId > 0 ? record.recordId : fallbackLine;
    entry.physicalLines = 1;
    return entry;
}

int EventLogProvider::prependBatch(const QVector<EventLogRecord> &newestFirst)
{
    if (newestFirst.isEmpty())
        return 0;

    std::vector<LogEntry> newEntries;
    newEntries.reserve(static_cast<size_t>(newestFirst.size()));

    qint64 newest = m_newestRecordId;
    qint64 oldest = m_oldestRecordId;
    for (const EventLogRecord &record : newestFirst) {
        // Defensive against an overlap between two polls; IDs are monotonic
        // within a channel unless it was cleared (handled by the watcher).
        if (record.recordId > 0 && record.recordId <= m_newestRecordId)
            continue;
        newEntries.push_back(makeEntry(record, 0));
        if (record.recordId > newest)
            newest = record.recordId;
        if (record.recordId > 0 && (oldest == 0 || record.recordId < oldest))
            oldest = record.recordId;
    }

    if (newEntries.empty())
        return 0;

    m_entries.insert(m_entries.begin(), newEntries.begin(), newEntries.end());
    m_newestRecordId = newest;
    m_oldestRecordId = oldest;
    m_info.lineCount = static_cast<int>(m_entries.size());

    if (m_countsComputed) {
        for (const LogEntry &entry : newEntries)
            m_levelCounts[logLevelIndex(entry.level)] += 1;
    }

    return static_cast<int>(newEntries.size());
}

DocumentInfo EventLogProvider::documentInfo() const
{
    DocumentInfo info = m_info;
    info.formatName = QCoreApplication::translate("EventLog", "Windows Event Log");
    return info;
}

const LogEntry &EventLogProvider::entryAt(int row) const
{
    static const LogEntry empty;
    if (row < 0 || row >= static_cast<int>(m_entries.size()))
        return empty;
    return m_entries[static_cast<size_t>(row)];
}

QVector<int> EventLogProvider::levelCounts() const
{
    if (m_countsComputed)
        return m_levelCounts;

    m_levelCounts = QVector<int>(kLogLevelCount, 0);
    for (const LogEntry &entry : m_entries)
        m_levelCounts[logLevelIndex(entry.level)] += 1;
    m_countsComputed = true;
    return m_levelCounts;
}

QString EventLogProvider::sourceName(int sourceIndex) const
{
    Q_UNUSED(sourceIndex);
    return m_sourceName;
}

} // namespace lv
