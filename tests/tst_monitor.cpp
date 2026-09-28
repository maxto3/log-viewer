#include "core/LogSource.h"
#include "core/LogWatcher.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace lv;

/// Tests for live monitoring (spec.md REQ-MON): incremental index updates,
/// partial trailing lines, truncation and file rotation.
class TestMonitor : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void appendsNewLines();
    void completesPartialTrailingLine();
    void truncationTriggersRebuild();
    void rotationTriggersRebuild();
    void watcherStopsCleanly();

private:
    QString writeFile(const QByteArray &content);
    void appendToFile(const QByteArray &content);

    QTemporaryDir m_dir;
    QString m_path;
};

void TestMonitor::init()
{
    QVERIFY(m_dir.isValid());
    m_path = QDir(m_dir.path()).filePath(QStringLiteral("live.log"));
}

void TestMonitor::cleanup()
{
    QFile::remove(m_path);
}

QString TestMonitor::writeFile(const QByteArray &content)
{
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    file.write(content);
    file.close();
    return m_path;
}

void TestMonitor::appendToFile(const QByteArray &content)
{
    QFile file(m_path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
    file.write(content);
    file.flush();
    file.close();
}

void TestMonitor::appendsNewLines()
{
    QVERIFY(!writeFile("2026-09-28 10:00:00 INFO first\n").isEmpty());

    QString error;
    const auto source = LogSource::open(m_path, QString(), &error, 0);
    QVERIFY2(source != nullptr, qPrintable(error));
    QCOMPARE(source->rowCount(), 1);

    LogWatcher watcher;
    QSignalSpy appendedSpy(&watcher, &LogWatcher::rowsAppended);
    watcher.setInterval(50);
    watcher.start(source.get());

    appendToFile("2026-09-28 10:00:01 DEBUG second\n2026-09-28 10:00:02 ERROR third\n");

    QTRY_COMPARE_WITH_TIMEOUT(source->rowCount(), 3, 5000);
    QCOMPARE(appendedSpy.count(), 1);
    QCOMPARE(appendedSpy.first().at(0).toInt(), 1);   // first new row
    QCOMPARE(appendedSpy.first().at(1).toInt(), 2);   // count

    QCOMPARE(source->entryAt(1).message, QStringLiteral("second"));
    QCOMPARE(source->entryAt(1).level, LogLevel::Debug);
    QCOMPARE(source->entryAt(2).message, QStringLiteral("third"));
    QCOMPARE(source->entryAt(2).level, LogLevel::Error);
    watcher.stop();
}

void TestMonitor::completesPartialTrailingLine()
{
    // A writer that has not flushed the line terminator yet.
    QVERIFY(!writeFile("2026-09-28 10:00:00 INFO par").isEmpty());

    QString error;
    const auto source = LogSource::open(m_path, QString(), &error, 0);
    QVERIFY2(source != nullptr, qPrintable(error));
    QCOMPARE(source->rowCount(), 1);
    QCOMPARE(source->entryAt(0).message, QStringLiteral("par"));

    appendToFile("tial line\n");
    bool rebuilt = false;
    const int appended = source->refreshFromDisk(&rebuilt, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!rebuilt);
    QCOMPARE(appended, 0);                  // the same row was completed
    QCOMPARE(source->rowCount(), 1);
    QCOMPARE(source->entryAt(0).message, QStringLiteral("partial line"));
}

void TestMonitor::truncationTriggersRebuild()
{
    QString content;
    for (int i = 0; i < 20; ++i)
        content += QStringLiteral("2026-09-28 10:00:00 INFO line %1\n").arg(i);
    QVERIFY(!writeFile(content.toUtf8()).isEmpty());

    QString error;
    const auto source = LogSource::open(m_path, QString(), &error, 0);
    QVERIFY2(source != nullptr, qPrintable(error));
    QCOMPARE(source->rowCount(), 20);

    LogWatcher watcher;
    QSignalSpy rebuiltSpy(&watcher, &LogWatcher::documentRebuilt);
    watcher.setInterval(50);
    watcher.start(source.get());

    // Truncate and start over (log rotation by copytruncate).
    QVERIFY(!writeFile("2026-09-28 11:00:00 WARN fresh\n").isEmpty());

    QTRY_COMPARE_WITH_TIMEOUT(rebuiltSpy.count(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(source->rowCount(), 1, 5000);
    QCOMPARE(source->entryAt(0).message, QStringLiteral("fresh"));
    watcher.stop();
}

void TestMonitor::rotationTriggersRebuild()
{
    QVERIFY(!writeFile("2026-09-28 10:00:00 INFO old file\n"
                       "2026-09-28 10:00:01 INFO old line 2\n").isEmpty());

    QString error;
    const auto source = LogSource::open(m_path, QString(), &error, 0);
    QVERIFY2(source != nullptr, qPrintable(error));
    QCOMPARE(source->rowCount(), 2);

    LogWatcher watcher;
    QSignalSpy rebuiltSpy(&watcher, &LogWatcher::documentRebuilt);
    watcher.setInterval(50);
    watcher.start(source.get());

    // Replace the file (rename + create, as log rotation usually does).
    QVERIFY(QFile::remove(m_path));
    QVERIFY(!writeFile("2026-09-28 12:00:00 INFO rotated\n").isEmpty());

    QTRY_COMPARE_WITH_TIMEOUT(rebuiltSpy.count(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(source->rowCount(), 1, 5000);
    QCOMPARE(source->entryAt(0).message, QStringLiteral("rotated"));
    watcher.stop();
}

void TestMonitor::watcherStopsCleanly()
{
    QVERIFY(!writeFile("one\n").isEmpty());
    QString error;
    const auto source = LogSource::open(m_path, QString(), &error, 0);
    QVERIFY(source != nullptr);

    LogWatcher watcher;
    watcher.setInterval(50);
    watcher.start(source.get());
    QVERIFY(watcher.isActive());

    watcher.stop();
    QVERIFY(!watcher.isActive());

    appendToFile("two\n");
    QTest::qWait(200);
    QCOMPARE(source->rowCount(), 1);   // no polling after stop()
}

QTEST_MAIN(TestMonitor)
#include "tst_monitor.moc"
