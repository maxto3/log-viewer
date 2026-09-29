#include "platform/FileAssociation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace lv;

/// File association registration (spec.md REQ-ASSOC-06/07/08): the paths and
/// command lines written to the registry are pure functions, and the round trip
/// against HKCU is checked with a test-only extension so that the developer's
/// own .log association is never hijacked (REQ-ASSOC-05).
class TestFileAssociation : public QObject
{
    Q_OBJECT

private slots:
    void onlyLogIsRegistered();
    void progIdForExtension();
    void commandLineQuotesThePath();
    void defaultIconUsesIndexZero();
    void invalidExtensionIsRejected();
    void unregisteredExecutableIsReported();
    void unsupportedPlatformExplainsXdgMime();
    void registrationRoundTrip();
    void movedCopyIsCleanedUp();
};

void TestFileAssociation::onlyLogIsRegistered()
{
    // REQ-ASSOC-01: ".log" and nothing else -- not even ".txt".
    QCOMPARE(FileAssociation::defaultExtension(), QStringLiteral(".log"));
}

void TestFileAssociation::progIdForExtension()
{
    QCOMPARE(FileAssociation::progIdFor(QStringLiteral(".log")), QStringLiteral("LogViewer.log"));
    QCOMPARE(FileAssociation::progIdFor(QStringLiteral(".lvtest")),
             QStringLiteral("LogViewer.lvtest"));
}

void TestFileAssociation::commandLineQuotesThePath()
{
    // REQ-ASSOC-02: "%1" makes the double click open the file; the path is
    // quoted because it may contain spaces.
    const QString command = FileAssociation::commandLineFor(
        QStringLiteral("C:\\Program Files\\Log Viewer\\log-viewer.exe"));
    QCOMPARE(command, QStringLiteral("\"C:\\Program Files\\Log Viewer\\log-viewer.exe\" \"%1\""));
}

void TestFileAssociation::defaultIconUsesIndexZero()
{
    QCOMPARE(FileAssociation::defaultIconFor(QStringLiteral("C:\\log-viewer\\log-viewer.exe")),
             QStringLiteral("\"C:\\log-viewer\\log-viewer.exe\",0"));
}

void TestFileAssociation::invalidExtensionIsRejected()
{
    const FileAssociation::Result result = FileAssociation::registerForCurrentUser(
        QStringLiteral("log"), QStringLiteral("C:\\log-viewer\\log-viewer.exe"));
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("log")));
}

void TestFileAssociation::unregisteredExecutableIsReported()
{
    if (!FileAssociation::isSupported())
        QSKIP("Registration is Windows only (REQ-ASSOC-03 keeps Linux on xdg-mime).");

    const FileAssociation::Result result = FileAssociation::registerForCurrentUser(
        QStringLiteral(".lvtest"), QStringLiteral("C:\\log-viewer-does-not-exist\\log-viewer.exe"));
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("log-viewer-does-not-exist")));
}

void TestFileAssociation::unsupportedPlatformExplainsXdgMime()
{
    // REQ-ASSOC-03: Linux keeps using the xdg-mime helper script, and the
    // message the caller shows must say so.
    QVERIFY(FileAssociation::unsupportedText().contains(QStringLiteral("xdg-mime")));
}

void TestFileAssociation::registrationRoundTrip()
{
    if (!FileAssociation::isSupported())
        QSKIP("Registration is Windows only (REQ-ASSOC-03 keeps Linux on xdg-mime).");

    // A test-only extension: registering ".log" here would change the developer's
    // own file association (REQ-ASSOC-05 forbids silent default changes).
    const QString extension = QStringLiteral(".lvtest");
    const QString exePath = QCoreApplication::applicationFilePath();
    QVERIFY(QFileInfo::exists(exePath));

    // Clean up in case an earlier run died before unregistering.
    FileAssociation::unregisterForCurrentUser(extension);
    QVERIFY(!FileAssociation::isRegisteredForCurrentUser(extension, exePath));

    const FileAssociation::Result registered =
        FileAssociation::registerForCurrentUser(extension, exePath);
    QVERIFY2(registered.ok, qPrintable(registered.error));
    QVERIFY(FileAssociation::isRegisteredForCurrentUser(extension, exePath));

    // A different path must not be reported as registered: this is what makes
    // "the folder was moved" visible in the Settings menu (REQ-UI-16).
    QVERIFY(!FileAssociation::isRegisteredForCurrentUser(
        extension, exePath + QStringLiteral(".moved")));

    const FileAssociation::Result removed = FileAssociation::unregisterForCurrentUser(extension);
    QVERIFY2(removed.ok, qPrintable(removed.error));
    QVERIFY(!FileAssociation::isRegisteredForCurrentUser(extension, exePath));
}

void TestFileAssociation::movedCopyIsCleanedUp()
{
    if (!FileAssociation::isSupported())
        QSKIP("Registration is Windows only (REQ-ASSOC-03 keeps Linux on xdg-mime).");

    // "The folder was moved away and the old copy is gone": register a copy of
    // this test executable, delete it, then remove the association again. The
    // Applications key is named after the file, so the entry the old folder left
    // behind is the one that must not survive.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString copyPath = QDir(dir.path()).filePath(
        QFileInfo(QCoreApplication::applicationFilePath()).fileName());
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), copyPath));
    QVERIFY(QFileInfo::exists(copyPath));

    const QString extension = QStringLiteral(".lvtest");
    FileAssociation::unregisterForCurrentUser(extension);

    const FileAssociation::Result registered =
        FileAssociation::registerForCurrentUser(extension, copyPath);
    QVERIFY2(registered.ok, qPrintable(registered.error));
    QVERIFY(FileAssociation::isRegisteredForCurrentUser(extension, copyPath));

    QVERIFY(QFile::remove(copyPath));

    const FileAssociation::Result removed = FileAssociation::unregisterForCurrentUser(extension);
    QVERIFY2(removed.ok, qPrintable(removed.error));

    const QString applicationKey =
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\%1")
            .arg(QFileInfo(copyPath).fileName());
    QSettings applications(applicationKey, QSettings::NativeFormat);
    QVERIFY2(applications.childGroups().isEmpty(),
             "an \"Open with\" entry pointing at a deleted copy must be removed");
}

QTEST_MAIN(TestFileAssociation)
#include "tst_association.moc"
