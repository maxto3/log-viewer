#include "core/LogSource.h"

#include "core/LogFormatRegistry.h"
#include "core/TimestampParser.h"
#include "platform/EncodingBackend.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>

namespace lv {
namespace {
constexpr int kProbeLineCount = 200;
constexpr int kEncodingProbeBytes = 4096;

/// RAII helper for bulk passes: keeps the file open for the whole pass and
/// closes it afterwards, so a long lived handle can never block log rotation.
class BulkReadScope
{
public:
    explicit BulkReadScope(const LogSource *source)
        : m_source(source)
    {
        m_source->beginBulkRead();
    }
    ~BulkReadScope() { m_source->endBulkRead(); }
    BulkReadScope(const BulkReadScope &) = delete;
    BulkReadScope &operator=(const BulkReadScope &) = delete;

private:
    const LogSource *m_source;
};
} // namespace

std::shared_ptr<LogSource> LogSource::open(const QString &path, const QString &forcedFormatId,
                                           QString *errorMessage, int maxLines,
                                           bool mergeContinuations)
{
    auto source = std::shared_ptr<LogSource>(new LogSource);
    source->m_path = path;
    source->m_fileName = QFileInfo(path).fileName();
    source->m_maxLines = maxLines;
    source->m_mergeContinuations = mergeContinuations;

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LogSource", "File does not exist: %1").arg(path);
        return nullptr;
    }
    if (!info.isReadable()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LogSource", "File is not readable: %1").arg(path);
        return nullptr;
    }

    auto index = std::make_unique<LineIndex>();
    if (!index->build(path, maxLines, errorMessage))
        return nullptr;
    source->m_index = std::move(index);

    source->m_fileNameDate = dateFromFileName(source->m_fileName);
    source->m_modifiedDate = info.lastModified().date();
    source->detectEncoding();

    // Probe the head of the file to pick a format. The probe keeps the file open
    // for the whole sample: one open per line is needlessly slow (see
    // beginBulkRead()).
    QStringList sample;
    const int sampleCount = qMin(kProbeLineCount, source->m_index->lineCount());
    sample.reserve(sampleCount);
    source->beginBulkRead();
    for (int row = 0; row < sampleCount; ++row)
        sample.append(source->readLine(row));
    source->endBulkRead();

    const LogFormatRegistry &registry = LogFormatRegistry::instance();
    if (!forcedFormatId.isEmpty() && forcedFormatId != QLatin1String("auto")) {
        source->m_format = registry.findById(forcedFormatId);
        if (!source->m_format) {
            if (errorMessage)
                *errorMessage = QCoreApplication::translate("LogSource", "Unknown log format: %1")
                                    .arg(forcedFormatId);
            return nullptr;
        }
    } else {
        source->m_format = registry.detect(sample);
    }

    source->buildEntryIndex(0, maxLines);
    return source;
}

int LogSource::rowCount() const
{
    if (!m_documentEntries.empty())
        return static_cast<int>(m_documentEntries.size());
    return static_cast<int>(m_entryStart.size());
}

void LogSource::buildEntryIndex(int fromPhysicalLine, int maxLines)
{
    if (!m_index)
        return;

    // Every physical line of the range is read at least once; keep the file open
    // for that (one open per line dominated the open time of large files) and
    // release it when the pass is done (see beginBulkRead()).
    BulkReadScope bulk(this);

    // Structured documents (the event log XML export) are parsed as a whole
    // because one entry is not necessarily a range of lines.
    if (fromPhysicalLine == 0 && m_documentEntries.empty() && m_format && m_index
        && m_index->lineCount() > 0 && m_index->lineCount() <= kMaxDocumentIndexLines) {
        QStringList allLines;
        allLines.reserve(m_index->lineCount());
        for (int i = 0; i < m_index->lineCount(); ++i)
            allLines << readLine(i);

        std::vector<LogEntry> entries;
        if (m_format->parseDocument(allLines, entries) && !entries.empty()
            && entries.size() <= kMaxDocumentEntries) {
            if (m_format->numbersEntriesSequentially()) {
                for (size_t i = 0; i < entries.size(); ++i)
                    entries[i].firstLine = static_cast<qint64>(i) + 1;
            }
            m_documentEntries = std::move(entries);
            return;
        }
    }

    const int lines = m_index->lineCount();
    const int limit = maxLines > 0 ? maxLines : lines;
    for (int line = fromPhysicalLine; line < lines && line < limit; ++line) {
        const QString text = readLine(line);
        const bool isEntryStart = !m_format || m_format->startsEntry(text);

        if (isEntryStart) {
            m_entryStart.push_back(line);
            m_entryLines.push_back(1);
        } else if (m_entryStart.empty()) {
            // Leading non-entry lines (CSV headers, IIS directives) are skipped.
            continue;
        } else if (m_mergeContinuations) {
            ++m_entryLines.back();
        } else {
            m_entryStart.push_back(line);
            m_entryLines.push_back(1);
        }
    }
}

QByteArray LogSource::readRawLine(int row) const
{
    if (!m_index || row < 0 || row >= m_index->lineCount())
        return {};
    const qint64 start = m_index->lineStart(row);
    const int length = m_index->lineLength(row);
    if (length <= 0)
        return {};

    // During a bulk pass the file stays open for the whole pass; outside of one
    // the file is opened per read on purpose: holding a handle open for the whole
    // session would prevent log rotation (rename/replace) on Windows.
    if (m_bulkOpen) {
        if (!m_bulkFile->seek(start))
            return {};
        return m_bulkFile->read(length);
    }

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    if (!file.seek(start))
        return {};
    return file.read(length);
}

void LogSource::beginBulkRead() const
{
    if (m_bulkOpen)
        return;
    if (!m_bulkFile)
        m_bulkFile = std::make_unique<QFile>(m_path);
    else
        m_bulkFile->setFileName(m_path);
    m_bulkOpen = m_bulkFile->open(QIODevice::ReadOnly);
}

void LogSource::endBulkRead() const
{
    if (!m_bulkOpen)
        return;
    m_bulkFile->close();
    m_bulkOpen = false;
}

QString LogSource::readLine(int row, bool *ok) const
{
    const QByteArray raw = readRawLine(row);
    if (ok)
        *ok = !raw.isNull();
    return decodeBytes(raw);
}

QString LogSource::sourceName(int sourceIndex) const
{
    Q_UNUSED(sourceIndex);
    return m_fileName;
}

void LogSource::detectEncoding()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QByteArray head = file.read(kEncodingProbeBytes);
    file.close();

    if (head.size() >= 3 && static_cast<unsigned char>(head.at(0)) == 0xEF
        && static_cast<unsigned char>(head.at(1)) == 0xBB
        && static_cast<unsigned char>(head.at(2)) == 0xBF) {
        m_encoding = Encoding::Utf8;    // BOM is consumed by QStringDecoder
        return;
    }
    if (head.size() >= 2) {
        const unsigned char b0 = static_cast<unsigned char>(head.at(0));
        const unsigned char b1 = static_cast<unsigned char>(head.at(1));
        if (b0 == 0xFF && b1 == 0xFE) {
            m_encoding = Encoding::Utf16LE;
            return;
        }
        if (b0 == 0xFE && b1 == 0xFF) {
            m_encoding = Encoding::Utf16BE;
            return;
        }
    }

    // Heuristic: a high share of NUL bytes indicates UTF-16 text.
    if (!head.isEmpty()) {
        int zeros = 0;
        for (char c : head) {
            if (c == '\0')
                ++zeros;
        }
        if (zeros * 100 / head.size() > 20) {
            m_encoding = Encoding::Utf16LE;
            return;
        }
    }

    // Not UTF-16: is it valid UTF-8? If not, detect the legacy encoding once
    // (GB18030 → Big5 → Shift_JIS → CP1252, REQ-PARSE-04).
    QStringDecoder utf8(QStringConverter::Utf8);
    utf8(head);
    if (utf8.hasError())
        m_legacyEncoding = EncodingBackend::detectLegacyEncoding(head);
}

QString LogSource::decodeBytes(const QByteArray &raw) const
{
    if (raw.isEmpty())
        return QString();

    if (m_encoding != Encoding::Utf8) {
        const QStringConverter::Encoding mode =
            m_encoding == Encoding::Utf16LE ? QStringConverter::Utf16LE : QStringConverter::Utf16BE;
        QStringDecoder decoder(mode);
        QString text = decoder(raw);
        if (text.startsWith(QChar::ByteOrderMark))
            text.remove(0, 1);
        return text;
    }

    QStringDecoder decoder(QStringConverter::Utf8);
    QString text = decoder(raw);
    if (!decoder.hasError())
        return text;

    // Not valid UTF-8: use the detected legacy encoding (or the platform local
    // 8-bit encoding as the last resort, REQ-PARSE-04/05).
    m_decodingFallbackUsed = true;
    if (!m_legacyEncoding.isEmpty())
        return EncodingBackend::decode(raw, m_legacyEncoding).text;
    return EncodingBackend::decodeLocal(raw).text;
}

void LogSource::parseEntryAt(int row, LogEntry &entry) const
{
    entry = LogEntry();
    if (row < 0 || row >= static_cast<int>(m_entryStart.size()))
        return;

    const int first = m_entryStart[static_cast<size_t>(row)];
    const int count = m_entryLines[static_cast<size_t>(row)];
    QStringList lines;
    lines.reserve(count);
    for (int i = 0; i < count; ++i)
        lines << readLine(first + i);

    const bool parsed = m_format && m_format->parseEntry(lines, first + 1, m_mergeContinuations, entry);
    if (!parsed) {
        entry = LogEntry();
        entry.message = lines.join(QLatin1Char('\n'));
        entry.level = LogLevel::Other;
    }
    entry.firstLine = (m_format && m_format->numbersEntriesSequentially())
        ? row + 1        // record number: physical lines cannot identify the row
        : first + 1;     // first physical line of the entry
    entry.physicalLines = count;
    entry.sourceIndex = 0;
}

const LogEntry &LogSource::entryAt(int row) const
{
    if (!m_documentEntries.empty()) {
        static const LogEntry empty;
        if (row < 0 || row >= static_cast<int>(m_documentEntries.size()))
            return empty;
        return m_documentEntries[static_cast<size_t>(row)];
    }

    const auto cached = m_cache.constFind(row);
    if (cached != m_cache.constEnd())
        return *cached;
    if (m_cache.size() >= kMaxCachedEntries)
        m_cache.clear();

    LogEntry entry;
    parseEntryAt(row, entry);
    const auto inserted = m_cache.insert(row, entry);
    return *inserted;
}

QVector<int> LogSource::levelCounts() const
{
    if (m_countsComputed)
        return m_levelCounts;
    m_countsComputed = true;

    const int rows = rowCount();
    if (rows <= 0 || rows > kMaxCountScanLines)
        return {};

    m_levelCounts = QVector<int>(kLogLevelCount, 0);
    BulkReadScope bulk(this);
    for (int row = 0; row < rows; ++row) {
        const LogEntry &entry = entryAt(row);
        m_levelCounts[logLevelIndex(entry.level)] += 1;
    }
    return m_levelCounts;
}

int LogSource::refreshFromDisk(bool *rebuilt, QString *errorMessage)
{
    if (rebuilt)
        *rebuilt = false;
    if (!m_index)
        return 0;

    const int entriesBefore = rowCount();

    bool wasRebuilt = false;
    if (!m_index->refresh(m_path, m_maxLines, errorMessage, &wasRebuilt))
        return 0;
    if (rebuilt)
        *rebuilt = wasRebuilt;

    if (wasRebuilt) {
        m_entryStart.clear();
        m_entryLines.clear();
        m_documentEntries.clear();
        m_cache.clear();
        invalidateLevelCounts();
        buildEntryIndex(0, m_maxLines);
        return rowCount();
    }

    // Structured documents are re-parsed as a whole (XML logs are not appended
    // to line by line).
    if (!m_documentEntries.empty()) {
        m_documentEntries.clear();
        m_cache.clear();
        invalidateLevelCounts();
        buildEntryIndex(0, m_maxLines);
        return 0;
    }

    // The last entry may have gained continuation lines (or the trailing partial
    // line was completed): drop and regroup it together with the new lines.
    if (!m_entryStart.empty()) {
        const int lastStart = m_entryStart.back();
        m_entryStart.pop_back();
        m_entryLines.pop_back();
        m_cache.remove(static_cast<int>(m_entryStart.size()));
        buildEntryIndex(lastStart, m_maxLines);
    } else {
        buildEntryIndex(0, m_maxLines);
    }

    const int entriesAfter = rowCount();
    if (entriesAfter <= entriesBefore)
        return 0;

    if (m_countsComputed) {
        for (int row = entriesBefore; row < entriesAfter; ++row)
            m_levelCounts[logLevelIndex(entryAt(row).level)] += 1;
    }
    return entriesAfter - entriesBefore;
}

void LogSource::invalidateLevelCounts()
{
    m_countsComputed = false;
    m_levelCounts.clear();
}

DocumentInfo LogSource::documentInfo() const
{
    DocumentInfo info;
    info.filePath = m_path;
    info.fileName = m_fileName;
    info.encodingName = m_encoding != Encoding::Utf8
        ? (m_encoding == Encoding::Utf16LE ? QStringLiteral("UTF-16LE") : QStringLiteral("UTF-16BE"))
        : (!m_legacyEncoding.isEmpty()
               ? m_legacyEncoding
               : (m_decodingFallbackUsed ? EncodingBackend::localEncodingName()
                                         : QStringLiteral("UTF-8")));
    if (m_format) {
        info.formatId = m_format->id();
        info.formatName = m_format->displayName();
    }
    if (m_index) {
        info.fileSize = m_index->fileSize();
        info.lineCount = rowCount();
        info.truncated = m_index->isTruncated();
    }
    info.mergeContinuations = m_mergeContinuations;
    return info;
}

QDate LogSource::fallbackDate() const
{
    if (m_fileNameDate.isValid())
        return m_fileNameDate;
    return m_modifiedDate;
}

} // namespace lv
