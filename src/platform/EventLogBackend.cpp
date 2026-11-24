#include "core/IEventLogBackend.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>

#include <algorithm>
#include <string>
#include <vector>

#if defined(Q_OS_WIN)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <winevt.h>
#endif

namespace lv {
namespace {

#if defined(Q_OS_WIN)

constexpr int kBatchSize = 128;
/// Upper bound for a formatted message buffer (characters); protects the UI
/// against publisher metadata that would ask for an absurd allocation.
constexpr DWORD kMaxMessageChars = 1u << 20;

/// Small RAII wrapper for EVT_HANDLE values.
class EventHandle
{
public:
    EventHandle() = default;
    explicit EventHandle(EVT_HANDLE handle) : m_handle(handle) {}
    ~EventHandle() { reset(); }

    EventHandle(const EventHandle &) = delete;
    EventHandle &operator=(const EventHandle &) = delete;

    EventHandle(EventHandle &&other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }
    EventHandle &operator=(EventHandle &&other) noexcept
    {
        if (this != &other) {
            reset();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    EVT_HANDLE get() const { return m_handle; }
    bool valid() const { return m_handle != nullptr; }

private:
    void reset()
    {
        if (m_handle) {
            EvtClose(m_handle);
            m_handle = nullptr;
        }
    }

    EVT_HANDLE m_handle = nullptr;
};

QString win32Message(DWORD code)
{
    wchar_t *buffer = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                                            | FORMAT_MESSAGE_FROM_SYSTEM
                                            | FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr, code, 0,
                                        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString text;
    if (length > 0 && buffer)
        text = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
    if (buffer)
        LocalFree(buffer);
    return text;
}

QString readErrorText(DWORD code, const QString &target)
{
    switch (code) {
    case ERROR_ACCESS_DENIED:
        return QCoreApplication::translate(
                   "EventLogBackend",
                   "Access denied while reading \"%1\". This event log requires administrator "
                   "privileges; use \"Open as Administrator…\" or restart the application as "
                   "administrator.")
            .arg(target);
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return QCoreApplication::translate("EventLogBackend", "\"%1\" was not found.").arg(target);
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_BUSY:
        return QCoreApplication::translate(
                   "EventLogBackend",
                   "The event log file \"%1\" is in use by the Windows Event Log service. Open the "
                   "channel instead, or copy the file while the service is stopped.")
            .arg(target);
    case ERROR_EVT_CHANNEL_NOT_FOUND:
        return QCoreApplication::translate("EventLogBackend", "The event log channel \"%1\" was not found.")
            .arg(target);
    default:
        break;
    }

    return QCoreApplication::translate("EventLogBackend",
                                       "Could not read \"%1\" (Windows error %2: %3).")
        .arg(target)
        .arg(code)
        .arg(win32Message(code));
}

EventLogError errorKindFor(DWORD code)
{
    switch (code) {
    case ERROR_ACCESS_DENIED:
        return EventLogError::AccessDenied;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_EVT_CHANNEL_NOT_FOUND:
        return EventLogError::NotFound;
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_BUSY:
        return EventLogError::InUse;
    default:
        return EventLogError::Other;
    }
}

void setError(EventLogReadResult *result, DWORD code, const QString &target)
{
    result->error = readErrorText(code, target);
    result->errorKind = errorKindFor(code);
}

/// Extracts `<EventRecordID>` from the rendered event XML.
qint64 eventRecordIdFromXml(const QString &xml)
{
    const int start = xml.indexOf(QLatin1String("<EventRecordID>"));
    if (start < 0)
        return 0;
    const int valueStart = start + 15;   // strlen("<EventRecordID>")
    const int end = xml.indexOf(QLatin1String("</EventRecordID>"), valueStart);
    if (end < 0)
        return 0;
    bool ok = false;
    const qint64 value = xml.mid(valueStart, end - valueStart).trimmed().toLongLong(&ok);
    return ok ? value : 0;
}

/// Renders an event as XML (System + EventData, no Message element).
bool renderXml(EVT_HANDLE event, QString *xml)
{
    DWORD used = 0;
    DWORD properties = 0;
    if (EvtRender(nullptr, event, EvtRenderEventXml, 0, nullptr, &used, &properties)) {
        xml->clear();
        return true;
    }
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || used == 0)
        return false;

    std::vector<wchar_t> buffer(static_cast<size_t>(used) / sizeof(wchar_t) + 1, L'\0');
    if (!EvtRender(nullptr, event, EvtRenderEventXml,
                   static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), buffer.data(), &used,
                   &properties))
        return false;

    *xml = QString::fromWCharArray(buffer.data());
    return true;
}

/// Extracts the provider name from the rendered event XML (`<Provider Name='…'>`).
QString providerNameFromXml(const QString &xml)
{
    const int provider = xml.indexOf(QLatin1String("<Provider"));
    if (provider < 0)
        return {};
    const int name = xml.indexOf(QLatin1String("Name="), provider);
    if (name < 0)
        return {};
    const int valueStart = name + 5;   // strlen("Name=")
    if (valueStart >= xml.size())
        return {};
    const QChar quote = xml.at(valueStart);
    if (quote != QLatin1Char('\'') && quote != QLatin1Char('"'))
        return {};
    const int end = xml.indexOf(quote, valueStart + 1);
    if (end < 0)
        return {};
    return xml.mid(valueStart + 1, end - valueStart - 1);
}

/// Formats the localized event message through the publisher metadata. The
/// metadata handle is cached per provider: loading it for every event would
/// dominate the open time of a large channel.
QString formatMessage(EVT_HANDLE event, const QString &xml,
                      QHash<QString, EVT_HANDLE> *metadata)
{
    const QString provider = providerNameFromXml(xml);
    if (provider.isEmpty())
        return {};

    auto it = metadata->find(provider);
    EVT_HANDLE handle = it == metadata->end() ? nullptr : it.value();
    if (it == metadata->end()) {
        const std::wstring wideProvider = provider.toStdWString();
        handle = EvtOpenPublisherMetadata(nullptr, wideProvider.c_str(), nullptr, 0, 0);
        metadata->insert(provider, handle);
    }
    if (!handle)
        return {};

    // Most messages fit into the reusable buffer, so the common case needs one
    // EvtFormatMessage call instead of a size query + a second call.
    thread_local std::vector<wchar_t> buffer(4096, L'\0');
    DWORD written = 0;
    if (!EvtFormatMessage(handle, event, 0, 0, nullptr, EvtFormatMessageEvent,
                          static_cast<DWORD>(buffer.size()), buffer.data(), &written)) {
        const DWORD code = GetLastError();
        if (code != ERROR_INSUFFICIENT_BUFFER || written == 0)
            return {};
        buffer.assign(static_cast<size_t>(qMin(written, kMaxMessageChars)) + 1, L'\0');
        if (!EvtFormatMessage(handle, event, 0, 0, nullptr, EvtFormatMessageEvent,
                              static_cast<DWORD>(buffer.size()), buffer.data(), &written))
            return {};
    }
    return QString::fromWCharArray(buffer.data());
}

EventLogReadResult readTarget(const QString &target, bool isFile, int maxRecords,
                              QHash<QString, EVT_HANDLE> *metadata,
                              const QString &xpath = QString())
{
    EventLogReadResult result;

    const QString path = isFile ? QFileInfo(target).absoluteFilePath() : target;
    const DWORD queryFlags = (isFile ? EvtQueryFilePath : EvtQueryChannelPath)
        | EvtQueryReverseDirection;
    EventHandle queryHandle(EvtQuery(nullptr, path.toStdWString().c_str(),
                                     xpath.isEmpty() ? nullptr : xpath.toStdWString().c_str(),
                                     queryFlags));
    if (!queryHandle.valid()) {
        setError(&result, GetLastError(), target);
        return result;
    }

    // Ask for one record more than the limit to know whether the log continues.
    const int fetchLimit = maxRecords > 0 ? maxRecords + 1 : 0;
    int fetched = 0;
    bool failed = false;

    std::vector<EVT_HANDLE> batch(static_cast<size_t>(kBatchSize), nullptr);

    while (!failed && (fetchLimit <= 0 || fetched < fetchLimit)) {
        DWORD request = static_cast<DWORD>(kBatchSize);
        if (fetchLimit > 0)
            request = static_cast<DWORD>(qMin<int>(kBatchSize, fetchLimit - fetched));

        DWORD returned = 0;
        if (!EvtNext(queryHandle.get(), request, batch.data(), 0, 0, &returned)) {
            const DWORD code = GetLastError();
            if (code != ERROR_NO_MORE_ITEMS)
                setError(&result, code, target);
            break;
        }
        if (returned == 0)
            break;

        for (DWORD index = 0; index < returned; ++index) {
            EventHandle event(batch[static_cast<size_t>(index)]);
            ++fetched;
            if (maxRecords > 0 && fetched > maxRecords) {
                result.truncated = true;
                continue;   // the extra record is closed by the wrapper
            }

            QString xml;
            if (!renderXml(event.get(), &xml)) {
                setError(&result, GetLastError(), target);
                failed = true;
                // Release the rest of this batch; leaving them open would leak
                // one EVT_HANDLE per remaining record.
                for (DWORD rest = index + 1; rest < returned; ++rest)
                    EvtClose(batch[static_cast<size_t>(rest)]);
                break;
            }

            EventLogRecord record;
            record.xml = xml;
            record.recordId = eventRecordIdFromXml(xml);
            record.message = formatMessage(event.get(), xml, metadata);
            result.records.append(record);
        }
    }

    // The query and the table both use newest first (REQ-EVT-06): the first
    // row is the latest event and "load more" appends older records below.
    return result;
}

QVector<EventLogChannel> enumerateChannels(QString *error)
{
    QVector<EventLogChannel> channels;

    EventHandle enumeration(EvtOpenChannelEnum(nullptr, 0));
    if (!enumeration.valid()) {
        if (error)
            *error = readErrorText(GetLastError(), QStringLiteral("Windows Event Log"));
        return channels;
    }

    std::vector<wchar_t> buffer(1024, L'\0');
    while (true) {
        DWORD used = 0;
        if (!EvtNextChannelPath(enumeration.get(), static_cast<DWORD>(buffer.size()), buffer.data(),
                                &used)) {
            const DWORD code = GetLastError();
            if (code == ERROR_NO_MORE_ITEMS)
                break;
            if (error)
                *error = readErrorText(code, QStringLiteral("Windows Event Log"));
            break;
        }
        channels.append({QString::fromWCharArray(buffer.data())});
    }

    std::sort(channels.begin(), channels.end(),
              [](const EventLogChannel &left, const EventLogChannel &right) {
                  return QString::compare(left.name, right.name, Qt::CaseInsensitive) < 0;
              });
    return channels;
}

class WindowsEventLogBackend final : public IEventLogBackend
{
public:
    ~WindowsEventLogBackend() override
    {
        for (EVT_HANDLE handle : m_metadata) {
            if (handle)
                EvtClose(handle);
        }
    }

    bool isSupported() const override { return true; }

    QVector<EventLogChannel> channels(QString *error) const override
    {
        return enumerateChannels(error);
    }

    EventLogReadResult readChannel(const QString &channel, int maxRecords) const override
    {
        return readTarget(channel, false, maxRecords, &m_metadata);
    }

    EventLogReadResult readChannelSince(const QString &channel, qint64 afterRecordId,
                                        int maxRecords) const override
    {
        if (afterRecordId <= 0)
            return readTarget(channel, false, maxRecords, &m_metadata);

        // Reverse direction keeps the newest-first display order; the filter
        // excludes what the document already shows (REQ-EVT-11).
        const QString query = QStringLiteral("*[System[EventRecordID > %1]]").arg(afterRecordId);
        return readTarget(channel, false, maxRecords, &m_metadata, query);
    }

    EventLogReadResult readFile(const QString &path, int maxRecords) const override
    {
        return readTarget(path, true, maxRecords, &m_metadata);
    }

private:
    /// Publisher metadata is expensive to load; keep it for the lifetime of the
    /// backend so reopening a channel does not reload every resource DLL.
    mutable QHash<QString, EVT_HANDLE> m_metadata;
};

#else   // !Q_OS_WIN

class UnsupportedEventLogBackend final : public IEventLogBackend
{
public:
    bool isSupported() const override { return false; }

    QVector<EventLogChannel> channels(QString *error) const override
    {
        if (error)
            *error = unsupportedText();
        return {};
    }

    EventLogReadResult readChannel(const QString &, int) const override { return unsupported(); }
    EventLogReadResult readChannelSince(const QString &, qint64, int) const override
    {
        return unsupported();
    }
    EventLogReadResult readFile(const QString &, int) const override { return unsupported(); }

private:
    static QString unsupportedText()
    {
        return QCoreApplication::translate(
            "EventLogBackend", "Windows event logs are only available on Windows.");
    }

    static EventLogReadResult unsupported()
    {
        EventLogReadResult result;
        result.error = unsupportedText();
        return result;
    }
};

#endif

} // namespace

EventLogBackendPtr createEventLogBackend()
{
#if defined(Q_OS_WIN)
    return std::make_shared<WindowsEventLogBackend>();
#else
    return std::make_shared<UnsupportedEventLogBackend>();
#endif
}

} // namespace lv
