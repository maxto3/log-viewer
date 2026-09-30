#include "platform/ElevatedFileReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>

#if defined(Q_OS_LINUX)
#  include <unistd.h>
#endif

using namespace lv;

namespace {

/// Captures the stdout/stderr file descriptors for the lifetime of the object,
/// so the in-process helper entry point can be tested like a subprocess.
#if defined(Q_OS_LINUX)
class StreamCapture
{
public:
    StreamCapture()
    {
        std::fflush(stdout);
        std::fflush(stderr);
        m_savedOut = ::dup(STDOUT_FILENO);
        m_savedErr = ::dup(STDERR_FILENO);
        m_outFile = std::tmpfile();
        m_errFile = std::tmpfile();
        if (m_outFile)
            ::dup2(::fileno(m_outFile), STDOUT_FILENO);
        if (m_errFile)
            ::dup2(::fileno(m_errFile), STDERR_FILENO);
    }

    ~StreamCapture()
    {
        std::fflush(stdout);
        std::fflush(stderr);
        if (m_savedOut >= 0)
            ::dup2(m_savedOut, STDOUT_FILENO);
        if (m_savedErr >= 0)
            ::dup2(m_savedErr, STDERR_FILENO);
        if (m_savedOut >= 0)
            ::close(m_savedOut);
        if (m_savedErr >= 0)
            ::close(m_savedErr);
        if (m_outFile)
            std::fclose(m_outFile);
        if (m_errFile)
            std::fclose(m_errFile);
    }

    QByteArray stdoutBytes() const { return readAll(m_outFile); }
    QByteArray stderrBytes() const { return readAll(m_errFile); }

private:
    static QByteArray readAll(std::FILE *file)
    {
        if (!file)
            return {};
        std::fflush(file);
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        QByteArray data;
        if (size > 0)
            data.resize(static_cast<int>(size));
        const size_t read = std::fread(data.data(), 1, static_cast<size_t>(data.size()), file);
        data.resize(static_cast<int>(read));
        return data;
    }

    int m_savedOut = -1;
    int m_savedErr = -1;
    std::FILE *m_outFile = nullptr;
    std::FILE *m_errFile = nullptr;
};
#endif

/// RAII reset for the ElevatedFileReader test seam.
class ProgramOverrideGuard
{
public:
    ~ProgramOverrideGuard() { ElevatedFileReader::setProgramOverrideForTesting(QString()); }
};

} // namespace

/// Elevated read support (spec.md REQ-REL-04 / REQ-CLI-11): the internal
/// streaming helper and the parent-side pkexec snapshot flow. The tests use a
/// script as a stand-in for pkexec, so no authentication dialog is shown.
class TestElevated : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void supportFollowsPkexecPresence();
    void streamHelperWritesRawBytes();
    void streamHelperRejectsDirectoriesAndMissingFiles();
    void snapshotCopiesThroughHelperProgram();
    void snapshotFailureReportsErrorAndCleansUp();
    void snapshotRejectsNonRegularFiles();

private:
    QString writeFile(const QString &name, const QByteArray &content);
    QString writeExecutable(const QString &name, const QString &body);
    static QString runtimeBase();

    QTemporaryDir m_dir;
};

void TestElevated::init()
{
    QVERIFY(m_dir.isValid());
}

QString TestElevated::writeFile(const QString &name, const QByteArray &content)
{
    const QString path = QDir(m_dir.path()).filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    if (file.write(content) != content.size())
        return {};
    return path;
}

QString TestElevated::writeExecutable(const QString &name, const QString &body)
{
    const QString path = writeFile(name, QByteArray("#!/bin/sh\n") + body.toUtf8());
    if (path.isEmpty())
        return {};
    if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                         | QFileDevice::ExeOwner))
        return {};
    return path;
}

QString TestElevated::runtimeBase()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return base;
}

void TestElevated::supportFollowsPkexecPresence()
{
#if defined(Q_OS_LINUX)
    // Without an override the support flag mirrors the pkexec installation.
    const bool expected = !QStandardPaths::findExecutable(QStringLiteral("pkexec")).isEmpty();
    QCOMPARE(ElevatedFileReader::isSupported(), expected);
#else
    QVERIFY(!ElevatedFileReader::isSupported());
    QVERIFY(!ElevatedFileReader::unsupportedText().isEmpty());
#endif
}

void TestElevated::streamHelperWritesRawBytes()
{
#if defined(Q_OS_LINUX)
    // REQ-CLI-11: raw bytes (including NUL and invalid UTF-8) reach stdout
    // unchanged, even though the helper runs without a GUI or event loop.
    QByteArray payload = QByteArray::fromHex("000102fffe41");
    payload += "plain text line\n";
    payload += QByteArray::fromHex("0d0a");
    const QString path = writeFile(QStringLiteral("raw payload.bin"), payload);
    QVERIFY(!path.isEmpty());

    int result = -1;
    QByteArray captured;
    {
        StreamCapture capture;
        result = ElevatedFileReader::runStreamHelper(path);
        captured = capture.stdoutBytes();
    }
    QCOMPARE(result, 0);
    QCOMPARE(captured, payload);
#else
    QSKIP("POSIX file descriptors are not available on this platform");
#endif
}

void TestElevated::streamHelperRejectsDirectoriesAndMissingFiles()
{
#if defined(Q_OS_LINUX)
    int result = 0;
    QByteArray errors;
    {
        StreamCapture capture;
        result = ElevatedFileReader::runStreamHelper(m_dir.path());
        errors = capture.stderrBytes();
    }
    QVERIFY(result != 0);
    QVERIFY(!errors.isEmpty());

    result = 0;
    {
        StreamCapture capture;
        result = ElevatedFileReader::runStreamHelper(
            QDir(m_dir.path()).filePath(QStringLiteral("missing.log")));
    }
    QVERIFY(result != 0);
#else
    QSKIP("POSIX file descriptors are not available on this platform");
#endif
}

void TestElevated::snapshotCopiesThroughHelperProgram()
{
#if defined(Q_OS_LINUX)
    // The script stands in for pkexec: it receives the app path, the internal
    // flag and the source path and streams the source to stdout.
    const QByteArray payload = "elevated one\nline two\n";
    const QString source = writeFile(QStringLiteral("source log.log"), payload);
    QVERIFY(!source.isEmpty());
    const QString helper = writeExecutable(QStringLiteral("stream-helper.sh"),
                                           QStringLiteral("cat \"$3\"\n"));
    QVERIFY(!helper.isEmpty());
    ProgramOverrideGuard guard;
    ElevatedFileReader::setProgramOverrideForTesting(helper);

    QString error;
    const QString snapshot = ElevatedFileReader::createSnapshot(source, &error);
    QVERIFY2(!snapshot.isEmpty(), qPrintable(error));

    // The snapshot keeps the original file name so the window title stays
    // meaningful; it is stored in a private per-user directory.
    QCOMPARE(QFileInfo(snapshot).fileName(), QFileInfo(source).fileName());
    const QString directory = QFileInfo(snapshot).absolutePath();
    QVERIFY(directory.startsWith(runtimeBase()));
    const QFile::Permissions dirPermissions = QFile::permissions(directory);
    QVERIFY(dirPermissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(dirPermissions.testFlag(QFileDevice::WriteOwner));
    QVERIFY(dirPermissions.testFlag(QFileDevice::ExeOwner));
    QVERIFY(!dirPermissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!dirPermissions.testFlag(QFileDevice::ReadOther));
    const QFile::Permissions filePermissions = QFile::permissions(snapshot);
    QVERIFY(filePermissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(!filePermissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!filePermissions.testFlag(QFileDevice::ReadOther));

    QFile snapshotFile(snapshot);
    QVERIFY(snapshotFile.open(QIODevice::ReadOnly));
    QCOMPARE(snapshotFile.readAll(), payload);
    snapshotFile.close();

    ElevatedFileReader::removeSnapshot(snapshot);
    QVERIFY(!QFile::exists(snapshot));
    QVERIFY(!QFile::exists(directory));
#else
    QSKIP("Elevated snapshots are Linux only");
#endif
}

void TestElevated::snapshotFailureReportsErrorAndCleansUp()
{
#if defined(Q_OS_LINUX)
    // Snapshot directories are per-user and named log-viewer-elevated-*: compare
    // the listing before and after the failed attempt (no leftovers).
    const QDir base(runtimeBase());
    const QStringList leftovers = base.entryList({QStringLiteral("log-viewer-elevated-*")},
                                                  QDir::Dirs | QDir::Hidden);

    const QString source = writeFile(QStringLiteral("failure.log"), "data\n");
    QVERIFY(!source.isEmpty());
    const QString helper = writeExecutable(QStringLiteral("failing-helper.sh"),
                                           QStringLiteral("echo 'denied by helper' >&2\nexit 7\n"));
    QVERIFY(!helper.isEmpty());
    ProgramOverrideGuard guard;
    ElevatedFileReader::setProgramOverrideForTesting(helper);

    QString error;
    const QString snapshot = ElevatedFileReader::createSnapshot(source, &error);
    QVERIFY(snapshot.isEmpty());
    QVERIFY2(error.contains(QStringLiteral("denied by helper")), qPrintable(error));

    const QStringList afterFailure = base.entryList({QStringLiteral("log-viewer-elevated-*")},
                                                    QDir::Dirs | QDir::Hidden);
    QCOMPARE(afterFailure, leftovers);
#else
    QSKIP("Elevated snapshots are Linux only");
#endif
}

void TestElevated::snapshotRejectsNonRegularFiles()
{
#if defined(Q_OS_LINUX)
    const QString helper = writeExecutable(QStringLiteral("never-run.sh"), QStringLiteral("exit 0\n"));
    QVERIFY(!helper.isEmpty());
    ProgramOverrideGuard guard;
    ElevatedFileReader::setProgramOverrideForTesting(helper);

    QString error;
    QCOMPARE(ElevatedFileReader::createSnapshot(m_dir.path(), &error), QString());
    QVERIFY(!error.isEmpty());

    error.clear();
    QCOMPARE(ElevatedFileReader::createSnapshot(
                 QDir(m_dir.path()).filePath(QStringLiteral("missing.log")), &error),
             QString());
    QVERIFY(!error.isEmpty());
#else
    QSKIP("Elevated snapshots are Linux only");
#endif
}

QTEST_MAIN(TestElevated)
#include "tst_elevated.moc"
