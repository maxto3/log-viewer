#include "core/EventLogWatcher.h"

#include "core/EventLogProvider.h"

namespace lv {

EventLogWatcher::EventLogWatcher(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &EventLogWatcher::poll);
}

void EventLogWatcher::start(const EventLogBackendPtr &backend,
                            const std::shared_ptr<EventLogProvider> &provider, int intervalMs)
{
    stop();
    if (!backend || !backend->isSupported() || !provider || provider->channelName().isEmpty())
        return;

    m_backend = backend;
    m_provider = provider;
    m_channel = provider->channelName();
    m_added = 0;
    m_active = true;
    m_timer.setInterval(qMax(250, intervalMs));
    m_timer.start();
}

void EventLogWatcher::stop()
{
    m_timer.stop();
    m_active = false;
    m_backend.reset();
    m_provider.reset();
    m_channel.clear();
}

void EventLogWatcher::setInterval(int milliseconds)
{
    m_timer.setInterval(qMax(250, milliseconds));
}

void EventLogWatcher::poll()
{
    if (!m_active || !m_backend || !m_provider)
        return;

    const qint64 newest = m_provider->newestRecordId();
    const bool emptyDocument = m_provider->rowCount() == 0;
    if (newest <= 0 && !emptyDocument) {
        stop();
        emit watchingFailed(tr("The event log has no EventRecordID; live monitoring is unavailable."));
        return;
    }

    // Probe the current maximum ID first: a lower value means the channel was
    // cleared/recreated and the document has to be rebuilt (REQ-EVT-11).
    const EventLogReadResult probe = m_backend->readChannel(m_channel, 1);
    if (!probe.error.isEmpty()) {
        stop();
        emit watchingFailed(probe.error);
        return;
    }
    if (probe.records.isEmpty()) {
        // An empty channel means it was cleared; rebuild to drop the stale
        // document. An already empty document just waits for the first event.
        if (!emptyDocument) {
            stop();
            emit documentRebuilt();
        }
        return;
    }

    const qint64 probeId = probe.records.first().recordId;
    if (probeId <= 0) {
        stop();
        emit watchingFailed(tr("The event log has no EventRecordID; live monitoring is unavailable."));
        return;
    }
    if (!emptyDocument && probeId < newest) {
        stop();
        emit documentRebuilt();
        return;
    }
    if (probeId == newest)
        return;   // no new records

    const EventLogReadResult delta =
        m_backend->readChannelSince(m_channel, emptyDocument ? 0 : newest, kBatchLimit);
    if (!delta.error.isEmpty()) {
        stop();
        emit watchingFailed(delta.error);
        return;
    }

    const int added = m_provider->prependBatch(delta.records);
    if (added <= 0)
        return;

    m_added += added;
    emit recordsPrepended(added);

    if (m_added >= kMaxAddedRecords) {
        stop();
        emit limitReached();
    }
}

} // namespace lv
