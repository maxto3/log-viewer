#include "core/LogDocument.h"

#include "core/LogSource.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>

namespace lv {

LogDocument::OpenResult LogDocument::open(const QStringList &paths, const QString &forcedFormatId,
                                          int maxLines, bool mergeContinuations)
{
    OpenResult result;
    if (paths.isEmpty()) {
        result.error = QCoreApplication::translate("LogDocument", "No file was given.");
        return result;
    }

    auto document = std::shared_ptr<LogDocument>(new LogDocument);
    QString firstFormatId;
    QString firstFormatName;

    for (const QString &path : paths) {
        QString error;
        auto source = LogSource::open(path, forcedFormatId, &error, maxLines, mergeContinuations);
        if (!source) {
            result.error = error;
            return result;
        }

        const DocumentInfo info = source->documentInfo();
        if (firstFormatId.isEmpty()) {
            firstFormatId = info.formatId;
            firstFormatName = info.formatName;
        } else if (info.formatId != firstFormatId) {
            // Merging is only allowed for files of the same format (REQ-FILE-04).
            result.formatMismatch = true;
            result.error = QCoreApplication::translate(
                               "LogDocument",
                               "Cannot merge '%1' (%2) with the already opened files (%3).")
                               .arg(QFileInfo(path).fileName(),
                                    info.formatName.isEmpty() ? info.formatId : info.formatName,
                                    firstFormatName.isEmpty() ? firstFormatId : firstFormatName);
            return result;
        }
        document->m_sources.push_back(std::move(source));
    }

    document->buildOrder();
    result.document = document;
    result.timeMergeDisabled = !document->m_timeSorted && document->m_sources.size() > 1;
    return result;
}

std::shared_ptr<LogDocument> LogDocument::openSingle(const QString &path,
                                                     const QString &forcedFormatId,
                                                     QString *errorMessage, int maxLines,
                                                     bool mergeContinuations)
{
    const OpenResult result = open({path}, forcedFormatId, maxLines, mergeContinuations);
    if (!result.document && errorMessage)
        *errorMessage = result.error;
    return result.document;
}

const LogSource *LogDocument::source(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_sources.size()))
        return nullptr;
    return m_sources[static_cast<size_t>(index)].get();
}

std::shared_ptr<LogSource> LogDocument::sourcePtr(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_sources.size()))
        return nullptr;
    return m_sources[static_cast<size_t>(index)];
}

void LogDocument::buildOrder()
{
    m_order.clear();
    m_timeSorted = true;

    // A source without valid timestamps prevents time interleaving.
    for (const auto &source : m_sources) {
        const int rows = source->rowCount();
        if (rows == 0)
            continue;
        const LogEntry &first = source->entryAt(0);
        if (!first.time.isValid())
            m_timeSorted = false;
    }

    if (m_sources.size() == 1) {
        const int rows = m_sources.front()->rowCount();
        m_order.reserve(static_cast<size_t>(rows));
        for (int row = 0; row < rows; ++row)
            m_order.push_back({0, row});
        return;
    }

    if (!m_timeSorted) {
        // No usable timestamps: concatenate in the order the files were given.
        for (size_t sourceIndex = 0; sourceIndex < m_sources.size(); ++sourceIndex) {
            const int rows = m_sources[sourceIndex]->rowCount();
            for (int row = 0; row < rows; ++row)
                m_order.push_back({static_cast<int>(sourceIndex), row});
        }
        return;
    }

    // k-way merge by timestamp; ties keep the source order stable.
    std::vector<int> positions(m_sources.size(), 0);
    m_order.reserve(static_cast<size_t>(qMax(0, 4096)));
    while (true) {
        int bestSource = -1;
        QDateTime bestTime;
        for (size_t sourceIndex = 0; sourceIndex < m_sources.size(); ++sourceIndex) {
            const int position = positions[sourceIndex];
            if (position >= m_sources[sourceIndex]->rowCount())
                continue;
            const LogEntry &entry = m_sources[sourceIndex]->entryAt(position);
            if (bestSource < 0 || entry.time < bestTime) {
                bestSource = static_cast<int>(sourceIndex);
                bestTime = entry.time;
            }
        }
        if (bestSource < 0)
            break;
        m_order.push_back({bestSource, positions[static_cast<size_t>(bestSource)]});
        positions[static_cast<size_t>(bestSource)] += 1;
    }
}

const LogEntry &LogDocument::entryAt(int row) const
{
    static const LogEntry empty;
    if (row < 0 || row >= static_cast<int>(m_order.size()))
        return empty;
    const SourceRef &ref = m_order[static_cast<size_t>(row)];
    m_scratch = m_sources[static_cast<size_t>(ref.source)]->entryAt(ref.row);
    m_scratch.sourceIndex = ref.source;
    return m_scratch;
}

QVector<int> LogDocument::levelCounts() const
{
    if (m_countsComputed)
        return m_levelCounts;
    m_countsComputed = true;

    const int rows = rowCount();
    if (rows <= 0)
        return {};

    m_levelCounts = QVector<int>(kLogLevelCount, 0);
    for (int row = 0; row < rows; ++row)
        m_levelCounts[logLevelIndex(entryAt(row).level)] += 1;
    return m_levelCounts;
}

QString LogDocument::sourceName(int sourceIndex) const
{
    const LogSource *source = this->source(sourceIndex);
    if (!source)
        return {};
    return source->documentInfo().fileName;
}

DocumentInfo LogDocument::documentInfo() const
{
    DocumentInfo info;
    if (m_sources.empty())
        return info;

    if (m_sources.size() == 1) {
        info = m_sources.front()->documentInfo();
        info.multiFile = false;
        return info;
    }

    const DocumentInfo first = m_sources.front()->documentInfo();
    info.filePath = first.filePath;
    info.fileName = QCoreApplication::translate("LogDocument", "%1 + %2 more files")
                        .arg(first.fileName)
                        .arg(static_cast<int>(m_sources.size()) - 1);
    info.encodingName = first.encodingName;
    info.formatId = first.formatId;
    info.formatName = first.formatName;
    info.multiFile = true;
    info.lineCount = rowCount();
    info.mergeContinuations = first.mergeContinuations;
    for (const auto &source : m_sources)
        info.fileSize += source->documentInfo().fileSize;
    return info;
}

} // namespace lv
