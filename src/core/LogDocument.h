#pragma once

#include "core/IEntryProvider.h"

#include <QStringList>
#include <memory>
#include <vector>

namespace lv {

class LogSource;

/// A document made of one or more log files of the same format
/// (spec.md REQ-FILE-04/05).
///
/// Sources with parsable timestamps are merged by time; otherwise the sources
/// are concatenated in the order they were opened. Every entry keeps the index
/// of its source so the File column and the details pane can show it.
class LogDocument : public IEntryProvider
{
public:
    struct OpenResult {
        std::shared_ptr<LogDocument> document;
        QString error;
        bool formatMismatch = false;
        bool timeMergeDisabled = false;
    };

    /// Opens \a paths (at least one). All files must share the same log format.
    static OpenResult open(const QStringList &paths, const QString &forcedFormatId,
                           int maxLines, bool mergeContinuations);

    /// Convenience overload for a single file.
    static std::shared_ptr<LogDocument> openSingle(const QString &path, const QString &forcedFormatId,
                                                   QString *errorMessage, int maxLines,
                                                   bool mergeContinuations);

    int rowCount() const override { return static_cast<int>(m_order.size()); }
    const LogEntry &entryAt(int row) const override;
    QVector<int> levelCounts() const override;
    DocumentInfo documentInfo() const override;
    QString sourceName(int sourceIndex) const override;

    int sourceCount() const { return static_cast<int>(m_sources.size()); }
    const LogSource *source(int index) const;
    /// Shared ownership access (live monitoring of a single file document).
    std::shared_ptr<LogSource> sourcePtr(int index) const;

    /// A single file document: the caller can enable live monitoring for it.
    bool isSingleFile() const { return m_sources.size() == 1; }

private:
    LogDocument() = default;
    void buildOrder();

    struct SourceRef {
        int source = 0;
        int row = 0;
    };

    std::vector<std::shared_ptr<LogSource>> m_sources;
    std::vector<SourceRef> m_order;
    bool m_timeSorted = false;
    /// Entry returned to the model: a copy carrying the document-level source
    /// index used by the File column and the details pane.
    mutable LogEntry m_scratch;
    mutable QVector<int> m_levelCounts;
    mutable bool m_countsComputed = false;
};

} // namespace lv
