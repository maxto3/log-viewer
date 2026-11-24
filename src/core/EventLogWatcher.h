#pragma once

#include "core/IEventLogBackend.h"

#include <QObject>
#include <QTimer>
#include <QString>

#include <memory>

namespace lv {

class EventLogProvider;

/// Polls one live Windows event log channel for new records (REQ-EVT-11).
///
/// Every interval it probes the newest EventRecordID, reads the delta with
/// `readChannelSince()`, prepends the records to the provider (newest first)
/// and emits row counts for the model/view. A channel that was cleared or
/// recreated (record ID went backwards) emits documentRebuilt(); after
/// kMaxAddedRecords the watcher stops and emits limitReached().
class EventLogWatcher : public QObject
{
    Q_OBJECT

public:
    explicit EventLogWatcher(QObject *parent = nullptr);

    void start(const EventLogBackendPtr &backend,
               const std::shared_ptr<EventLogProvider> &provider, int intervalMs = 2000);
    void stop();
    bool isActive() const { return m_active; }

    void setInterval(int milliseconds);
    int interval() const { return m_timer.interval(); }

    /// Performs one update cycle immediately (also used by the tests).
    void poll();

    static constexpr int kMaxAddedRecords = 50000;
    static constexpr int kBatchLimit = 500;

signals:
    void recordsPrepended(int count);
    /// The channel was cleared/recreated (EventRecordID went backwards).
    void documentRebuilt();
    void watchingFailed(const QString &message);
    /// kMaxAddedRecords new events were added; polling stopped.
    void limitReached();

private:
    QTimer m_timer;
    EventLogBackendPtr m_backend;
    std::shared_ptr<EventLogProvider> m_provider;
    QString m_channel;
    int m_added = 0;
    bool m_active = false;
};

} // namespace lv
