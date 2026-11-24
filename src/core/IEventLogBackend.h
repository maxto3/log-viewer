#pragma once

#include <QString>
#include <QVector>

#include <memory>

namespace lv {

/// One event rendered by the native Windows event log backend.
struct EventLogRecord {
    /// `EvtRender(EvtRenderEventXml)` output. It contains the `<System>` and
    /// `<EventData>` sections but no readable `Message`.
    QString xml;
    /// `EvtFormatMessage` output (localized by the publisher metadata). Empty
    /// when the provider has no metadata or formatting failed; callers fall
    /// back to the EventData values of \a xml.
    QString message;
    /// `<EventRecordID>`; 0 when the XML did not contain one.
    qint64 recordId = 0;
};

/// A selectable Windows event log channel.
struct EventLogChannel {
    QString name;
};

/// Coarse error class so the UI can offer elevation without matching localized
/// error strings.
enum class EventLogError {
    None,
    AccessDenied,
    NotFound,
    InUse,
    Other,
};

struct EventLogReadResult {
    /// Newest record first (the backend queries in reverse direction), so the
    /// table shows the latest event in its first row (REQ-EVT-06).
    QVector<EventLogRecord> records;
    QString error;
    EventLogError errorKind = EventLogError::None;
    /// True when more records exist than the requested limit.
    bool truncated = false;
};

/// Read-only view of the native Windows event log API.
///
/// The interface lives in the core library so the provider and its tests can
/// run on every platform; the implementation is in `src/platform/` and is only
/// functional on Windows (spec.md REQ-EVT-04 / REQ-PLAT-01).
class IEventLogBackend
{
public:
    virtual ~IEventLogBackend() = default;

    virtual bool isSupported() const = 0;

    /// All channels of the local machine (enabled and disabled), sorted by
    /// name. Failures set \a error and return an empty list.
    virtual QVector<EventLogChannel> channels(QString *error) const = 0;

    /// Reads at most \a maxRecords of \a channel, newest first. \a maxRecords
    /// <= 0 means "all".
    virtual EventLogReadResult readChannel(const QString &channel, int maxRecords) const = 0;

    /// Reads records newer than \a afterRecordId (REQ-EVT-11). The result is
    /// newest first and excluded records are not re-formatted.
    virtual EventLogReadResult readChannelSince(const QString &channel, qint64 afterRecordId,
                                                int maxRecords) const = 0;

    /// Reads an archived/copied `.evtx` file through `EvtQueryFilePath`,
    /// newest first.
    virtual EventLogReadResult readFile(const QString &path, int maxRecords) const = 0;
};

using EventLogBackendPtr = std::shared_ptr<IEventLogBackend>;

/// Creates the platform backend. Non-Windows platforms return an unsupported
/// stub so the UI can hide the entry without platform conditionals.
EventLogBackendPtr createEventLogBackend();

} // namespace lv
