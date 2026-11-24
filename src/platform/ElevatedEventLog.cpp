#include "platform/ElevatedEventLog.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QFileInfo>
#include <QStringList>
#include <QUuid>

#include <cstdio>
#include <memory>
#include <vector>

#if defined(Q_OS_WIN)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  include <sddl.h>
#endif

namespace lv::ElevatedEventLog {
namespace {

bool g_useRunasForTesting = true;
QString g_programOverrideForTesting;

EventLogReadResult failRead(const QString &message,
                            EventLogError kind = EventLogError::Other)
{
    EventLogReadResult result;
    result.error = message;
    result.errorKind = kind;
    return result;
}

#if defined(Q_OS_WIN)

constexpr quint32 kPipeMagic = 0x4C56454Cu;   // "LVEL"
constexpr quint16 kPipeVersion = 1;
constexpr int kMaxFrameBytes = 128 * 1024 * 1024;
constexpr DWORD kConnectTimeoutMs = 30000;
constexpr DWORD kReadTimeoutMs = 30000;

enum FrameType : quint16 {
    FrameRecord = 2,
    FrameDone = 3,
    FrameError = 4,
};

struct HandleCloser
{
    void operator()(void *handle) const
    {
        if (handle && handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};
using HandlePtr = std::unique_ptr<void, HandleCloser>;

struct LocalFreeCloser
{
    void operator()(void *pointer) const
    {
        if (pointer)
            LocalFree(pointer);
    }
};
using LocalPtr = std::unique_ptr<void, LocalFreeCloser>;

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

QString protocolError(const QString &details)
{
    return QCoreApplication::translate("ElevatedEventLog",
                                       "The elevated helper returned an invalid response (%1).")
        .arg(details);
}

QString currentUserSid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};
    HandlePtr tokenGuard(token);

    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (size == 0)
        return {};

    std::vector<BYTE> buffer(size);
    if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &size))
        return {};

    const auto *user = reinterpret_cast<const TOKEN_USER *>(buffer.data());
    LPWSTR sidText = nullptr;
    QString result;
    if (ConvertSidToStringSidW(user->User.Sid, &sidText)) {
        result = QString::fromWCharArray(sidText);
        LocalFree(sidText);
    }
    return result;
}

/// Quotes one argument for the Windows command line (ShellExecuteEx lpParameters).
QString quoteArg(const QString &argument)
{
    QString result = QStringLiteral("\"");
    int backslashes = 0;
    for (const QChar character : argument) {
        if (character == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (character == QLatin1Char('"')) {
            result += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            backslashes = 0;
            result += QLatin1Char('"');
            continue;
        }
        result += QString(backslashes, QLatin1Char('\\'));
        backslashes = 0;
        result += character;
    }
    result += QString(backslashes * 2, QLatin1Char('\\'));
    result += QLatin1Char('"');
    return result;
}

bool writeAll(HANDLE pipe, const QByteArray &data)
{
    const char *cursor = data.constData();
    int remaining = data.size();
    while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(pipe, cursor, static_cast<DWORD>(remaining), &written, nullptr)
            || written == 0) {
            return false;
        }
        cursor += written;
        remaining -= static_cast<int>(written);
    }
    return true;
}

bool writeFrame(HANDLE pipe, quint16 type, const QByteArray &payload)
{
    QByteArray frame;
    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << kPipeMagic << kPipeVersion << type << quint32(payload.size());
    frame.append(payload);
    return writeAll(pipe, frame);
}

class PipeReader
{
public:
    explicit PipeReader(HANDLE pipe)
        : m_pipe(pipe)
        , m_event(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    ~PipeReader()
    {
        if (m_event)
            CloseHandle(m_event);
    }

    bool readChunk(QByteArray *buffer, bool *eof, QString *error)
    {
        if (!m_event) {
            *error = QCoreApplication::translate("ElevatedEventLog",
                                                  "Could not create the pipe reader event.");
            return false;
        }

        ResetEvent(m_event);
        m_overlapped = {};
        m_overlapped.hEvent = m_event;

        char chunk[64 * 1024];
        DWORD read = 0;
        if (!ReadFile(m_pipe, chunk, sizeof(chunk), &read, &m_overlapped)) {
            const DWORD code = GetLastError();
            if (code == ERROR_BROKEN_PIPE || code == ERROR_HANDLE_EOF) {
                *eof = true;
                return true;
            }
            if (code != ERROR_IO_PENDING) {
                *error = QCoreApplication::translate("ElevatedEventLog",
                                                      "Could not read from the elevated helper "
                                                      "(Windows error %1: %2).")
                             .arg(code)
                             .arg(win32Message(code));
                return false;
            }
            if (WaitForSingleObject(m_event, kReadTimeoutMs) != WAIT_OBJECT_0) {
                *error = QCoreApplication::translate(
                    "ElevatedEventLog", "The elevated helper did not respond in time.");
                return false;
            }
            if (!GetOverlappedResult(m_pipe, &m_overlapped, &read, FALSE)) {
                const DWORD resultCode = GetLastError();
                if (resultCode == ERROR_BROKEN_PIPE || resultCode == ERROR_HANDLE_EOF) {
                    *eof = true;
                    return true;
                }
                *error = QCoreApplication::translate("ElevatedEventLog",
                                                      "Could not read from the elevated helper "
                                                      "(Windows error %1: %2).")
                             .arg(resultCode)
                             .arg(win32Message(resultCode));
                return false;
            }
        }

        if (read == 0) {
            *eof = true;
            return true;
        }
        buffer->append(chunk, static_cast<int>(read));
        return true;
    }

private:
    HANDLE m_pipe = INVALID_HANDLE_VALUE;
    HANDLE m_event = nullptr;
    OVERLAPPED m_overlapped{};
};

EventLogError errorKindFromInt(qint32 value)
{
    switch (value) {
    case static_cast<qint32>(EventLogError::AccessDenied):
        return EventLogError::AccessDenied;
    case static_cast<qint32>(EventLogError::NotFound):
        return EventLogError::NotFound;
    case static_cast<qint32>(EventLogError::InUse):
        return EventLogError::InUse;
    case static_cast<qint32>(EventLogError::Other):
        return EventLogError::Other;
    default:
        return EventLogError::Other;
    }
}

EventLogReadResult readFrames(HANDLE pipe)
{
    EventLogReadResult result;
    PipeReader reader(pipe);
    QByteArray inbound;
    bool eof = false;

    const auto ensureBytes = [&](int needed) {
        while (inbound.size() < needed && !eof) {
            if (!reader.readChunk(&inbound, &eof, &result.error))
                return false;
        }
        return inbound.size() >= needed;
    };

    while (true) {
        if (!ensureBytes(12)) {
            if (result.error.isEmpty())
                result.error = QCoreApplication::translate(
                    "ElevatedEventLog", "The elevated helper exited before sending a response.");
            return result;
        }

        const QByteArray headerBytes = inbound.left(12);
        QDataStream header(headerBytes);
        header.setVersion(QDataStream::Qt_6_0);
        header.setByteOrder(QDataStream::LittleEndian);
        quint32 magic = 0;
        quint16 version = 0;
        quint16 type = 0;
        quint32 length = 0;
        header >> magic >> version >> type >> length;

        if (magic != kPipeMagic || version != kPipeVersion) {
            result.error = protocolError(QStringLiteral("magic/version"));
            return result;
        }
        if (length > static_cast<quint32>(kMaxFrameBytes)) {
            result.error = protocolError(QStringLiteral("frame length"));
            return result;
        }

        if (!ensureBytes(12 + static_cast<int>(length))) {
            if (result.error.isEmpty())
                result.error = QCoreApplication::translate(
                    "ElevatedEventLog", "The elevated helper exited before sending a complete record.");
            return result;
        }

        const QByteArray payload = inbound.mid(12, static_cast<int>(length));
        inbound.remove(0, 12 + static_cast<int>(length));

        if (type == FrameDone)
            return result;

        if (type == FrameError) {
            QDataStream stream(payload);
            stream.setVersion(QDataStream::Qt_6_0);
            stream.setByteOrder(QDataStream::LittleEndian);
            qint32 kind = 0;
            QString message;
            stream >> kind >> message;
            if (stream.status() != QDataStream::Ok) {
                result.error = protocolError(QStringLiteral("error frame"));
                return result;
            }
            result.errorKind = errorKindFromInt(kind);
            result.error = message.isEmpty()
                ? QCoreApplication::translate("ElevatedEventLog",
                                              "The elevated helper could not read the event log.")
                : message;
            return result;
        }

        if (type != FrameRecord) {
            result.error = protocolError(QStringLiteral("unknown frame type %1").arg(type));
            return result;
        }

        QDataStream stream(payload);
        stream.setVersion(QDataStream::Qt_6_0);
        stream.setByteOrder(QDataStream::LittleEndian);
        qint64 recordId = 0;
        QString xml;
        QString message;
        stream >> recordId >> xml >> message;
        if (stream.status() != QDataStream::Ok) {
            result.error = protocolError(QStringLiteral("record frame"));
            return result;
        }
        result.records.append({xml, message, recordId});
    }
}

HANDLE createJobFor(HANDLE process)
{
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
        return nullptr;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION information{};
    information.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &information,
                            sizeof(information));
    if (!AssignProcessToJobObject(job, process)) {
        // Already in an incompatible job (some IDEs): KILL_ON_JOB_CLOSE cannot
        // be used, but the process is still waited for and closed below.
        CloseHandle(job);
        return nullptr;
    }
    return job;
}

#endif   // Q_OS_WIN

} // namespace

bool isSupported()
{
#if defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

QString unsupportedText()
{
#if defined(Q_OS_WIN)
    return {};
#else
    return QCoreApplication::translate(
        "ElevatedEventLog", "Elevated event log reads are only available on Windows.");
#endif
}

EventLogReadResult read(const QString &channel, const QString &path, int maxRecords,
                        QString *error)
{
    if (error)
        error->clear();

#if !defined(Q_OS_WIN)
    Q_UNUSED(channel);
    Q_UNUSED(path);
    Q_UNUSED(maxRecords);
    return failRead(unsupportedText());
#else
    if (channel.isEmpty() == path.isEmpty()) {
        const EventLogReadResult invalid = failRead(
            QCoreApplication::translate("ElevatedEventLog",
                                        "Exactly one event log channel or file is required."));
        if (error)
            *error = invalid.error;
        return invalid;
    }

    const QString exePath = g_programOverrideForTesting.isEmpty()
        ? QCoreApplication::applicationFilePath()
        : g_programOverrideForTesting;
    if (exePath.isEmpty()) {
        const EventLogReadResult missing = failRead(
            QCoreApplication::translate("ElevatedEventLog", "The application path is unavailable."));
        if (error)
            *error = missing.error;
        return missing;
    }

    const QString pipeName = QStringLiteral("\\\\.\\pipe\\log-viewer-evt-%1-%2")
                                 .arg(GetCurrentProcessId())
                                 .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    const QString userSid = currentUserSid();
    const QString sddl = userSid.isEmpty()
        ? QStringLiteral("D:P(A;;GA;;;OW)(A;;GA;;;BA)(A;;GA;;;SY)")
        : QStringLiteral("D:P(A;;GA;;;%1)(A;;GA;;;BA)(A;;GA;;;SY)").arg(userSid);
    PSECURITY_DESCRIPTOR securityDescriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl.toStdWString().c_str(), SDDL_REVISION_1, &securityDescriptor, nullptr)) {
        const DWORD code = GetLastError();
        const EventLogReadResult failed = failRead(
            QCoreApplication::translate("ElevatedEventLog",
                                        "Could not create the helper pipe ACL (Windows error %1: %2).")
                .arg(code)
                .arg(win32Message(code)));
        if (error)
            *error = failed.error;
        return failed;
    }
    LocalPtr securityGuard(securityDescriptor);

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = securityDescriptor;
    attributes.bInheritHandle = FALSE;

    HandlePtr pipe(CreateNamedPipeW(
        pipeName.toStdWString().c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
        64 * 1024, 64 * 1024, 0, &attributes));
    if (!pipe || pipe.get() == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        const EventLogReadResult failed = failRead(
            QCoreApplication::translate("ElevatedEventLog",
                                        "Could not create the helper pipe (Windows error %1: %2).")
                .arg(code)
                .arg(win32Message(code)));
        if (error)
            *error = failed.error;
        return failed;
    }

    const QString kind = channel.isEmpty() ? QStringLiteral("file") : QStringLiteral("channel");
    const QString target = channel.isEmpty() ? path : channel;
    const QString parameters = QStringLiteral("--event-log-stream %1 %2 %3 %4")
                                   .arg(quoteArg(pipeName), kind, quoteArg(target))
                                   .arg(maxRecords);
    const std::wstring parametersWide = parameters.toStdWString();
    const std::wstring exeWide = exePath.toStdWString();

    SHELLEXECUTEINFOW execute{};
    HANDLE childProcess = nullptr;
    if (g_useRunasForTesting) {
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_NOCLOSEPROCESS;
        execute.lpVerb = L"runas";
        execute.lpFile = exeWide.c_str();
        execute.lpParameters = parametersWide.c_str();
        execute.nShow = SW_HIDE;

        if (!ShellExecuteExW(&execute)) {
            const DWORD code = GetLastError();
            const EventLogReadResult failed = code == ERROR_CANCELLED
                ? failRead(QCoreApplication::translate("ElevatedEventLog",
                                                       "Authorization was cancelled."))
                : failRead(QCoreApplication::translate(
                               "ElevatedEventLog",
                               "Could not start the elevated helper (Windows error %1: %2).")
                               .arg(code)
                               .arg(win32Message(code)));
            if (error)
                *error = failed.error;
            return failed;
        }
        childProcess = execute.hProcess;
    } else {
        // Test seam only: run the same helper without UAC.
        std::wstring commandLineWide =
            (quoteArg(exePath) + QLatin1Char(' ') + parameters).toStdWString();
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION information{};
        if (!CreateProcessW(exeWide.c_str(), commandLineWide.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &information)) {
            const DWORD code = GetLastError();
            const EventLogReadResult failed = failRead(
                QCoreApplication::translate("ElevatedEventLog",
                                            "Could not start the helper (Windows error %1: %2).")
                    .arg(code)
                    .arg(win32Message(code)));
            if (error)
                *error = failed.error;
            return failed;
        }
        CloseHandle(information.hThread);
        childProcess = information.hProcess;
    }

    HandlePtr process(childProcess);
    HandlePtr job(process ? createJobFor(process.get()) : nullptr);

    OVERLAPPED connection{};
    HandlePtr connectionEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    connection.hEvent = connectionEvent.get();
    BOOL connected = ConnectNamedPipe(pipe.get(), &connection);
    if (!connected) {
        const DWORD code = GetLastError();
        if (code == ERROR_IO_PENDING) {
            if (WaitForSingleObject(connectionEvent.get(), kConnectTimeoutMs) != WAIT_OBJECT_0) {
                const EventLogReadResult failed = failRead(
                    QCoreApplication::translate("ElevatedEventLog",
                                                "The elevated helper did not connect in time."));
                if (error)
                    *error = failed.error;
                return failed;
            }
            DWORD transferred = 0;
            if (!GetOverlappedResult(pipe.get(), &connection, &transferred, FALSE)) {
                const DWORD resultCode = GetLastError();
                const EventLogReadResult failed = failRead(
                    QCoreApplication::translate("ElevatedEventLog",
                                                "Could not connect to the elevated helper "
                                                "(Windows error %1: %2).")
                        .arg(resultCode)
                        .arg(win32Message(resultCode)));
                if (error)
                    *error = failed.error;
                return failed;
            }
        } else if (code != ERROR_PIPE_CONNECTED) {
            const EventLogReadResult failed = failRead(
                QCoreApplication::translate("ElevatedEventLog",
                                            "Could not connect to the elevated helper "
                                            "(Windows error %1: %2).")
                    .arg(code)
                    .arg(win32Message(code)));
            if (error)
                *error = failed.error;
            return failed;
        }
    }

    EventLogReadResult result = readFrames(pipe.get());

    if (process)
        WaitForSingleObject(process.get(), 5000);
    process.reset();
    job.reset();   // KILL_ON_JOB_CLOSE terminates a stuck helper

    if (error)
        *error = result.error;
    return result;
#endif
}

void setUseRunasForTesting(bool useRunas)
{
    g_useRunasForTesting = useRunas;
}

void setProgramOverrideForTesting(const QString &program)
{
    g_programOverrideForTesting = program;
}

int runStreamHelper()
{
#if !defined(Q_OS_WIN)
    std::fprintf(stderr, "--event-log-stream is only available on Windows.\n");
    return 2;
#else
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return 2;
    LocalPtr argvGuard(argv);

    if (argc != 6) {
        std::fprintf(stderr,
                     "usage: log-viewer --event-log-stream <pipe> <channel|file> <target> "
                     "<maxRecords>\n");
        return 2;
    }

    const QString pipeName = QString::fromWCharArray(argv[2]);
    const QString kind = QString::fromWCharArray(argv[3]);
    const QString target = QString::fromWCharArray(argv[4]);
    bool ok = false;
    int maxRecords = QString::fromWCharArray(argv[5]).toInt(&ok);
    if (pipeName.isEmpty() || target.isEmpty() || !ok || maxRecords < 0
        || (kind != QLatin1String("channel") && kind != QLatin1String("file"))) {
        std::fprintf(stderr, "invalid --event-log-stream arguments\n");
        return 2;
    }

    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 50; ++attempt) {
        pipe = CreateFileW(pipeName.toStdWString().c_str(), GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() != ERROR_PIPE_BUSY)
            break;
        WaitNamedPipeW(pipeName.toStdWString().c_str(), 200);
    }
    if (pipe == INVALID_HANDLE_VALUE)
        return 1;
    HandlePtr pipeGuard(pipe);

    const EventLogBackendPtr backend = createEventLogBackend();
    if (!backend || !backend->isSupported()) {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << qint32(EventLogError::Other)
               << QStringLiteral("Windows event logs are only available on Windows.");
        writeFrame(pipe, FrameError, payload);
        return 1;
    }

    const EventLogReadResult result = kind == QLatin1String("channel")
        ? backend->readChannel(target, maxRecords)
        : backend->readFile(target, maxRecords);

    if (!result.error.isEmpty()) {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << qint32(result.errorKind) << result.error;
        writeFrame(pipe, FrameError, payload);
        return 1;
    }

    for (const EventLogRecord &record : result.records) {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << qint64(record.recordId) << record.xml << record.message;
        if (!writeFrame(pipe, FrameRecord, payload))
            return 1;
    }

    if (!writeFrame(pipe, FrameDone, {}))
        return 1;
    FlushFileBuffers(pipe);
    return 0;
#endif
}

} // namespace lv::ElevatedEventLog
