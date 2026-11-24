#pragma once

#include "core/IEntryProvider.h"
#include "core/IEventLogBackend.h"

#include <QString>
#include <QVector>

#include <memory>
#include <vector>

namespace lv {

/// Read-only document backed by the native Windows event log API.
///
/// Records are read once by the platform backend and stored as regular LogEntry
/// rows, newest first (REQ-EVT-06), so the whole display/filter/find stack works
/// unchanged.
class EventLogProvider : public IEntryProvider
{
public:
    /// Opening a channel loads at most this many of the newest events
    /// (REQ-EVT-06). The document is marked truncated when more exist.
    static constexpr int kMaxRecords = 20000;

    static std::shared_ptr<EventLogProvider> openChannel(const EventLogBackendPtr &backend,
                                                         const QString &channel, int maxRecords,
                                                         QString *error,
                                                         EventLogError *errorKind = nullptr);
    static std::shared_ptr<EventLogProvider> openFile(const EventLogBackendPtr &backend,
                                                      const QString &path, int maxRecords,
                                                      QString *error,
                                                      EventLogError *errorKind = nullptr);
    /// Builds a provider from records already read by another process (the
    /// elevated helper, REQ-EVT-10). The caller supplies the target labels.
    static std::shared_ptr<EventLogProvider> fromResult(const QString &channel,
                                                        const QString &fileName,
                                                        const QString &filePath, int maxRecords,
                                                        const EventLogReadResult &result);

    int rowCount() const override { return static_cast<int>(m_entries.size()); }
    const LogEntry &entryAt(int row) const override;
    QVector<int> levelCounts() const override;
    /// Returns the document metadata with the format name translated on demand,
    /// so switching the UI language updates the status bar (REQ-I18N-02).
    DocumentInfo documentInfo() const override;
    QString sourceName(int sourceIndex) const override;

    /// Channel name for channel documents, empty for `.evtx` files.
    QString channelName() const { return m_channel; }
    /// Limit used for the current document (0 = all records were requested).
    /// The UI uses it to offer "load more" on a truncated document.
    int requestedLimit() const { return m_requestedLimit; }

    /// Record IDs of the newest and oldest loaded event (0 when unknown).
    qint64 newestRecordId() const { return m_newestRecordId; }
    qint64 oldestRecordId() const { return m_oldestRecordId; }

    /// Inserts a batch of newer records at the top (REQ-EVT-11). \a newestFirst
    /// must come from EvtQueryReverseDirection, i.e. the first element is the
    /// latest event. Returns the number of inserted rows (or -1 on error).
    int prependBatch(const QVector<EventLogRecord> &newestFirst);

private:
    EventLogProvider() = default;

    void load(const QString &channel, const QString &fileName, const QString &filePath,
              int maxRecords, const EventLogReadResult &result);
    static LogEntry makeEntry(const EventLogRecord &record, qint64 fallbackLine);
    std::vector<LogEntry> m_entries;
    DocumentInfo m_info;
    QString m_channel;
    QString m_sourceName;
    int m_requestedLimit = 0;
    qint64 m_newestRecordId = 0;
    qint64 m_oldestRecordId = 0;
    mutable QVector<int> m_levelCounts;
    mutable bool m_countsComputed = false;
};

} // namespace lv
