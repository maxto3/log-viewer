#include "platform/ElevatedFileReader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(Q_OS_LINUX)
#  include <cerrno>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace lv {
namespace {

/// Test seam: when set, the snapshot flow runs this program instead of pkexec.
QString s_programOverride;

QString snapshotProgram()
{
#if defined(Q_OS_LINUX)
    if (!s_programOverride.isEmpty())
        return s_programOverride;
    return QStandardPaths::findExecutable(QStringLiteral("pkexec"));
#else
    return {};
#endif
}

/// Creates the private snapshot directory atomically with mode 0700
/// (mkdtemp) in the per-user runtime directory (fallback: temp).
#if defined(Q_OS_LINUX)
QString createPrivateDirectory(QString *errorMessage)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (base.isEmpty())
        base = QDir::tempPath();

    QByteArray pattern = QFile::encodeName(
        QDir(base).filePath(QStringLiteral("log-viewer-elevated-XXXXXX")));
    if (!::mkdtemp(pattern.data())) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate(
                                "ElevatedFileReader", "Cannot create the private snapshot directory: %1")
                                .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return {};
    }
    return QFile::decodeName(pattern.constData());
}
#endif

/// Snapshot file name derived from the source; never a directory reference.
QString snapshotFileName(const QFileInfo &source)
{
    const QString name = source.fileName();
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String(".."))
        return QStringLiteral("snapshot.log");
    return name;
}

QString failureText(int exitCode, const QByteArray &standardError, const QString &path)
{
    QString message;
    if (exitCode == 126)
        message = QCoreApplication::translate("ElevatedFileReader",
                                              "Authentication was cancelled or failed.");
    else if (exitCode == 127)
        message = QCoreApplication::translate(
                      "ElevatedFileReader", "Not authorized to read '%1' with elevated privileges.")
                      .arg(path);

    // Keep the helper output: it names missing polkit agents and similar
    // environment problems that the generic text cannot express.
    if (!standardError.isEmpty()) {
        const QString detail = QString::fromLocal8Bit(standardError);
        return message.isEmpty() ? detail : message + QLatin1Char(' ') + detail;
    }
    if (!message.isEmpty())
        return message;
    return QCoreApplication::translate("ElevatedFileReader",
                                       "The authentication helper failed (exit code %1).")
        .arg(exitCode);
}

void discardSnapshot(const QString &snapshot)
{
    QFile::remove(snapshot);
    QDir().rmdir(QFileInfo(snapshot).absolutePath());
}

} // namespace

bool ElevatedFileReader::isSupported()
{
#if defined(Q_OS_LINUX)
    return !snapshotProgram().isEmpty();
#else
    return false;
#endif
}

QString ElevatedFileReader::unsupportedText()
{
#if defined(Q_OS_LINUX)
    return QCoreApplication::translate(
        "ElevatedFileReader",
        "The system authentication helper (pkexec) is not available. Grant read access manually "
        "(for example add your user to the \"adm\" group) or copy the file to a readable location.");
#else
    return QCoreApplication::translate(
        "ElevatedFileReader",
        "Reading files with elevated privileges is only available on Linux.");
#endif
}

QString ElevatedFileReader::createSnapshot(const QString &path, QString *errorMessage,
                                           const QString &programOverride)
{
    if (errorMessage)
        errorMessage->clear();

#if defined(Q_OS_LINUX)
    const QString program = programOverride.isEmpty() ? snapshotProgram() : programOverride;
    if (program.isEmpty()) {
        if (errorMessage)
            *errorMessage = unsupportedText();
        return {};
    }

    const QFileInfo source(path);
    if (!source.exists() || !source.isFile()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("ElevatedFileReader",
                                                        "Not a regular file: %1").arg(path);
        return {};
    }

    // The snapshot lives in a directory only the current user can enter (0700,
    // created atomically); the elevated helper never writes here, it only
    // streams to stdout.
    QString directoryError;
    const QString directory = createPrivateDirectory(&directoryError);
    if (directory.isEmpty()) {
        if (errorMessage)
            *errorMessage = directoryError;
        return {};
    }

    // Keep the original file name so the window title stays meaningful.
    const QString snapshot = QDir(directory).filePath(snapshotFileName(source));
    QFile output(snapshot);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("ElevatedFileReader",
                                                        "Cannot create the temporary snapshot: %1")
                                .arg(output.errorString());
        discardSnapshot(snapshot);
        return {};
    }
    QFile::setPermissions(snapshot, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    // Fixed argv, no shell (REQ-PLAT-08 exception): pkexec launches this very
    // executable in the internal streaming helper mode.
    QProcess process;
    process.setProgram(program);
    process.setArguments({QCoreApplication::applicationFilePath(),
                          QStringLiteral("--elevated-stream"), path});
    process.start();
    if (!process.waitForStarted()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate(
                                "ElevatedFileReader", "Cannot start the system authentication helper: %1")
                                .arg(process.errorString());
        output.close();
        discardSnapshot(snapshot);
        return {};
    }

    // Drain stdout into the snapshot. On a write failure keep draining so the
    // helper can never block on a full pipe.
    bool writeFailed = false;
    QString writeError;
    while (process.waitForReadyRead(-1)) {
        const QByteArray chunk = process.readAllStandardOutput();
        if (chunk.isEmpty())
            continue;
        if (!writeFailed && output.write(chunk) != chunk.size()) {
            writeFailed = true;
            writeError = output.errorString();
        }
    }
    process.waitForFinished();
    const QByteArray tail = process.readAllStandardOutput();
    if (!tail.isEmpty() && !writeFailed && output.write(tail) != tail.size()) {
        writeFailed = true;
        writeError = output.errorString();
    }
    output.close();

    if (writeFailed) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("ElevatedFileReader",
                                                        "Cannot write the temporary snapshot: %1")
                                .arg(writeError);
        discardSnapshot(snapshot);
        return {};
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage)
            *errorMessage = failureText(process.exitCode(),
                                        process.readAllStandardError().trimmed(), path);
        discardSnapshot(snapshot);
        return {};
    }

    return snapshot;
#else
    Q_UNUSED(path);
    Q_UNUSED(programOverride);
    if (errorMessage)
        *errorMessage = unsupportedText();
    return {};
#endif
}

void ElevatedFileReader::removeSnapshot(const QString &snapshotPath)
{
    if (snapshotPath.isEmpty())
        return;
    discardSnapshot(snapshotPath);
}

int ElevatedFileReader::runStreamHelper(const QString &path)
{
#if defined(Q_OS_LINUX)
    const QByteArray localPath = QFile::encodeName(path);
    const int fd = ::open(localPath.constData(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        std::fprintf(stderr, "elevated-stream: cannot open '%s': %s\n", localPath.constData(),
                     std::strerror(errno));
        return 1;
    }

    // Follows symlinks (log rotation uses them) but rejects everything that is
    // not a regular file, checked on the open descriptor (no TOCTOU).
    struct stat info {};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        std::fprintf(stderr, "elevated-stream: not a regular file: '%s'\n", localPath.constData());
        ::close(fd);
        return 1;
    }

    std::array<char, 64 * 1024> buffer{};
    bool failed = false;
    for (;;) {
        const ssize_t bytesRead = ::read(fd, buffer.data(), buffer.size());
        if (bytesRead == 0)
            break;
        if (bytesRead < 0) {
            if (errno == EINTR)
                continue;
            std::fprintf(stderr, "elevated-stream: read failed: %s\n", std::strerror(errno));
            failed = true;
            break;
        }

        ssize_t written = 0;
        while (written < bytesRead) {
            const ssize_t result = ::write(STDOUT_FILENO,
                                           buffer.data() + written,
                                           static_cast<size_t>(bytesRead - written));
            if (result < 0) {
                if (errno == EINTR)
                    continue;
                // stdout closed (parent gone or write error): stop silently,
                // the exit code tells the parent that the stream is incomplete.
                failed = true;
                break;
            }
            written += result;
        }
        if (failed)
            break;
    }

    ::close(fd);
    return failed ? 1 : 0;
#else
    Q_UNUSED(path);
    std::fprintf(stderr, "elevated-stream: not supported on this platform\n");
    return 1;
#endif
}

void ElevatedFileReader::setProgramOverrideForTesting(const QString &program)
{
    s_programOverride = program;
}

} // namespace lv
