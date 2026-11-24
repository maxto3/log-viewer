#include "core/EventLogProvider.h"
#include "core/EventLogWatcher.h"
#include "core/FilterSpec.h"
#include "core/IEventLogBackend.h"
#include "core/LogLevel.h"
#include "core/LogTableModel.h"
#include "core/Matcher.h"
#include "platform/ElevatedEventLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTest>

using namespace lv;

namespace {

/// Builds the XML that `EvtRender(EvtRenderEventXml)` returns.
QString eventXml(int level, int eventId, const QString &provider = QStringLiteral("Test-Provider"),
                 const QString &data = QStringLiteral("value"))
{
    return QStringLiteral(
               "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'>"
               "<System><Provider Name='%1'/><EventID>%2</EventID><Level>%3</Level>"
               "<TimeCreated SystemTime='2026-11-23T10:00:00.1234567Z'/>"
               "<Computer>TEST-PC</Computer><Channel>System</Channel></System>"
               "<EventData><Data Name='Param'>%4</Data></EventData></Event>")
        .arg(provider)
        .arg(eventId)
        .arg(level)
        .arg(data);
}

QString eventXmlWithId(qint64 recordId, int level = 4)
{
    return QStringLiteral(
               "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'>"
               "<System><Provider Name='Test-Provider'/><EventID>1</EventID><Level>%1</Level>"
               "<EventRecordID>%2</EventRecordID>"
               "<TimeCreated SystemTime='2026-11-23T10:00:00Z'/>"
               "<Computer>TEST-PC</Computer><Channel>System</Channel></System>"
               "<EventData><Data Name='Param'>value</Data></EventData></Event>")
        .arg(level)
        .arg(recordId);
}

class MockEventLogBackend : public IEventLogBackend
{
public:
    bool supported = true;
    QVector<EventLogChannel> channelList;
    EventLogReadResult channelResult;
    EventLogReadResult fileResult;
    EventLogReadResult sinceResult;
    mutable QString lastChannel;
    mutable QString lastFile;
    mutable qint64 lastSince = -1;
    mutable int lastMaxRecords = -1;

    bool isSupported() const override { return supported; }

    QVector<EventLogChannel> channels(QString *error) const override
    {
        if (error)
            error->clear();
        return channelList;
    }

    EventLogReadResult readChannel(const QString &channel, int maxRecords) const override
    {
        lastChannel = channel;
        lastMaxRecords = maxRecords;
        return channelResult;
    }

    EventLogReadResult readChannelSince(const QString &channel, qint64 afterRecordId,
                                        int maxRecords) const override
    {
        lastChannel = channel;
        lastSince = afterRecordId;
        lastMaxRecords = maxRecords;
        return sinceResult;
    }

    EventLogReadResult readFile(const QString &path, int maxRecords) const override
    {
        lastFile = path;
        lastMaxRecords = maxRecords;
        return fileResult;
    }
};

} // namespace

/// Tests for the native Windows event log provider (spec.md REQ-EVT). The
/// backend is mocked so the mapping/cap/error behaviour runs on every platform.
class TestEventLog : public QObject
{
    Q_OBJECT

private slots:
    void mapsRenderedEventFields();
    void prefersFormattedMessageAndFallsBackToEventData();
    void marksTruncationAndCountsLevels();
    void documentInfoDescribesTheChannel();
    void openFileUsesTheFileName();
    void passesTheLimitToTheBackend();
    void tracksTheRequestedLimit();
    void mapsRecordIdToTheFirstColumn();
    void prependsNewRecordsAtTheTop();
    void watcherPrependsAndDetectsClear();
    void watcherFillsEmptyChannel();
    void modelPrependUpdatesRows();
    void modelPrependWithFilter();
    void readsThroughHelperWithoutUac();
    void propagatesBackendErrors();
    void rejectsUnsupportedBackend();
    void queriesTheApplicationChannelOnWindows();
};

void TestEventLog::mapsRenderedEventFields()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->channelResult.records = {
        {eventXml(2, 42), QStringLiteral("formatted message")},
        {eventXml(4, 7), QStringLiteral("second message")},
    };

    QString error;
    const auto provider = EventLogProvider::openChannel(backend, QStringLiteral("System"), 10, &error);
    QVERIFY2(provider != nullptr, qPrintable(error));
    QCOMPARE(provider->rowCount(), 2);
    QCOMPARE(provider->channelName(), QStringLiteral("System"));

    const LogEntry &first = provider->entryAt(0);
    QVERIFY(first.time.isValid());
    // EvtRender SystemTime is UTC; the provider converts it to local time.
    QCOMPARE(first.time.timeSpec(), Qt::LocalTime);
    QCOMPARE(first.time.toUTC().toString(Qt::ISODateWithMs),
             QStringLiteral("2026-11-23T10:00:00.123Z"));
    QCOMPARE(first.level, LogLevel::Error);
    QCOMPARE(first.rawLevel, QStringLiteral("ERROR"));
    QCOMPARE(first.target, QStringLiteral("Test-Provider"));
    QCOMPARE(first.host, QStringLiteral("TEST-PC"));
    QCOMPARE(first.message, QStringLiteral("formatted message"));
    QCOMPARE(first.extra.value(QCoreApplication::translate("Formats", "Event ID")),
             QStringLiteral("42"));
    QCOMPARE(first.firstLine, 1);
    QCOMPARE(first.physicalLines, 1);
    QCOMPARE(provider->entryAt(1).firstLine, 2);
}

void TestEventLog::prefersFormattedMessageAndFallsBackToEventData()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->channelResult.records = {
        {eventXml(4, 1, QStringLiteral("Test-Provider"), QStringLiteral("raw-value")),
         QStringLiteral("localized message")},
        {eventXml(4, 2, QStringLiteral("Test-Provider"), QStringLiteral("fallback-value")),
         QString()},
    };

    QString error;
    const auto provider = EventLogProvider::openChannel(backend, QStringLiteral("System"), 10, &error);
    QVERIFY2(provider != nullptr, qPrintable(error));
    QCOMPARE(provider->entryAt(0).message, QStringLiteral("localized message"));
    QVERIFY(provider->entryAt(1).message.contains(QStringLiteral("fallback-value")));
}

void TestEventLog::marksTruncationAndCountsLevels()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->channelResult.records = {
        {eventXml(2, 1), QString()},
        {eventXml(2, 2), QString()},
        {eventXml(3, 3), QString()},
    };
    backend->channelResult.truncated = true;

    QString error;
    const auto provider = EventLogProvider::openChannel(backend, QStringLiteral("System"), 3, &error);
    QVERIFY2(provider != nullptr, qPrintable(error));
    QVERIFY(provider->documentInfo().truncated);
    QCOMPARE(provider->rowCount(), 3);

    const QVector<int> counts = provider->levelCounts();
    QCOMPARE(counts.at(logLevelIndex(LogLevel::Error)), 2);
    QCOMPARE(counts.at(logLevelIndex(LogLevel::Warn)), 1);
}

void TestEventLog::documentInfoDescribesTheChannel()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->channelResult.records = {{eventXml(4, 1), QString()}};

    QString error;
    const auto provider =
        EventLogProvider::openChannel(backend, QStringLiteral("Application"), 20, &error);
    QVERIFY2(provider != nullptr, qPrintable(error));

    const DocumentInfo info = provider->documentInfo();
    QCOMPARE(info.fileName, QStringLiteral("Application"));
    QCOMPARE(info.formatId, QStringLiteral("wevt_native"));
    QVERIFY(!info.formatName.isEmpty());
    QCOMPARE(info.lineCount, 1);
    QVERIFY(info.filePath.isEmpty());
    QVERIFY(!info.multiFile);
    QCOMPARE(provider->sourceName(0), QStringLiteral("Application"));
    QCOMPARE(provider->sourceName(99), QStringLiteral("Application"));
}

void TestEventLog::openFileUsesTheFileName()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->fileResult.records = {{eventXml(4, 1), QString()}};

    QString error;
    const QString path = QStringLiteral("/var/tmp/archive.evtx");
    const auto provider = EventLogProvider::openFile(backend, path, 5, &error);
    QVERIFY2(provider != nullptr, qPrintable(error));
    QCOMPARE(backend->lastFile, path);

    const DocumentInfo info = provider->documentInfo();
    QCOMPARE(info.fileName, QStringLiteral("archive.evtx"));
    QCOMPARE(info.filePath, QFileInfo(path).absoluteFilePath());
    QVERIFY(provider->channelName().isEmpty());
    QCOMPARE(provider->sourceName(0), QStringLiteral("archive.evtx"));
}

void TestEventLog::passesTheLimitToTheBackend()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->channelResult.records = {{eventXml(4, 1), QString()}};
    backend->fileResult.records = {{eventXml(4, 1), QString()}};

    QString error;
    QVERIFY(EventLogProvider::openChannel(backend, QStringLiteral("System"),
                                          EventLogProvider::kMaxRecords, &error)
            != nullptr);
    QCOMPARE(backend->lastChannel, QStringLiteral("System"));
    QCOMPARE(backend->lastMaxRecords, EventLogProvider::kMaxRecords);

    QVERIFY(EventLogProvider::openFile(backend, QStringLiteral("archive.evtx"), 7, &error)
            != nullptr);
    QCOMPARE(backend->lastMaxRecords, 7);
}

void TestEventLog::tracksTheRequestedLimit()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->channelResult.records = {{eventXml(4, 1), QString()}};
    backend->fileResult.records = {{eventXml(4, 1), QString()}};

    QString error;
    const auto twentyK = EventLogProvider::openChannel(
        backend, QStringLiteral("System"), EventLogProvider::kMaxRecords, &error);
    QVERIFY2(twentyK != nullptr, qPrintable(error));
    QCOMPARE(twentyK->requestedLimit(), EventLogProvider::kMaxRecords);

    const auto all = EventLogProvider::openChannel(backend, QStringLiteral("System"), 0, &error);
    QVERIFY2(all != nullptr, qPrintable(error));
    QCOMPARE(all->requestedLimit(), 0);

    const auto file = EventLogProvider::openFile(backend, QStringLiteral("archive.evtx"), 40000,
                                                 &error);
    QVERIFY2(file != nullptr, qPrintable(error));
    QCOMPARE(file->requestedLimit(), 40000);
}

void TestEventLog::mapsRecordIdToTheFirstColumn()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->channelResult.records = {
        {eventXmlWithId(100), QString(), 100},
        {eventXmlWithId(99), QString(), 99},
    };

    QString error;
    const auto provider =
        EventLogProvider::openChannel(backend, QStringLiteral("System"), 10, &error);
    QVERIFY2(provider != nullptr, qPrintable(error));
    QCOMPARE(provider->rowCount(), 2);
    QCOMPARE(provider->entryAt(0).firstLine, 100);
    QCOMPARE(provider->entryAt(1).firstLine, 99);
    QCOMPARE(provider->newestRecordId(), 100);
    QCOMPARE(provider->oldestRecordId(), 99);
}

void TestEventLog::prependsNewRecordsAtTheTop()
{
    EventLogReadResult initial;
    initial.records = {
        {eventXmlWithId(100), QString(), 100},
        {eventXmlWithId(99), QString(), 99},
    };
    const auto provider = EventLogProvider::fromResult(
        QStringLiteral("System"), QStringLiteral("System"), QString(), 20000, initial);
    QVERIFY(provider != nullptr);

    const QVector<EventLogRecord> batch = {
        {eventXmlWithId(102), QString(), 102},
        {eventXmlWithId(101), QString(), 101},
    };
    QCOMPARE(provider->prependBatch(batch), 2);
    QCOMPARE(provider->rowCount(), 4);
    QCOMPARE(provider->entryAt(0).firstLine, 102);
    QCOMPARE(provider->entryAt(1).firstLine, 101);
    QCOMPARE(provider->entryAt(2).firstLine, 100);
    QCOMPARE(provider->entryAt(3).firstLine, 99);
    QCOMPARE(provider->newestRecordId(), 102);
    QCOMPARE(provider->oldestRecordId(), 99);
    QCOMPARE(provider->requestedLimit(), 20000);

    // IDs already shown are not inserted twice.
    QCOMPARE(provider->prependBatch(batch), 0);
    QCOMPARE(provider->rowCount(), 4);
}

void TestEventLog::watcherPrependsAndDetectsClear()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    EventLogReadResult initial;
    initial.records = {
        {eventXmlWithId(10), QString(), 10},
        {eventXmlWithId(9), QString(), 9},
    };
    const auto provider = EventLogProvider::fromResult(
        QStringLiteral("System"), QStringLiteral("System"), QString(), 20000, initial);
    QVERIFY(provider != nullptr);

    EventLogWatcher watcher;
    QSignalSpy prepended(&watcher, &EventLogWatcher::recordsPrepended);
    QSignalSpy rebuilt(&watcher, &EventLogWatcher::documentRebuilt);
    watcher.start(backend, provider, 60000);   // polling is driven manually

    backend->channelResult.records = {{eventXmlWithId(12), QString(), 12}};
    backend->sinceResult.records = {
        {eventXmlWithId(12), QString(), 12},
        {eventXmlWithId(11), QString(), 11},
    };
    watcher.poll();

    QCOMPARE(prepended.count(), 1);
    QCOMPARE(prepended.first().at(0).toInt(), 2);
    QCOMPARE(provider->rowCount(), 4);
    QCOMPARE(provider->newestRecordId(), 12);
    QCOMPARE(backend->lastSince, 10);

    // Same newest ID: nothing new.
    watcher.poll();
    QCOMPARE(prepended.count(), 1);

    // The channel was cleared and its IDs restarted below the document's newest.
    backend->channelResult.records = {{eventXmlWithId(5), QString(), 5}};
    watcher.poll();
    QCOMPARE(rebuilt.count(), 1);
    QVERIFY(!watcher.isActive());
}

void TestEventLog::watcherFillsEmptyChannel()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    EventLogReadResult empty;
    const auto provider = EventLogProvider::fromResult(
        QStringLiteral("System"), QStringLiteral("System"), QString(), 20000, empty);
    QCOMPARE(provider->rowCount(), 0);
    QCOMPARE(provider->newestRecordId(), 0);

    EventLogWatcher watcher;
    QSignalSpy prepended(&watcher, &EventLogWatcher::recordsPrepended);
    watcher.start(backend, provider, 60000);

    backend->channelResult.records = {{eventXmlWithId(5), QString(), 5}};
    backend->sinceResult.records = {{eventXmlWithId(5), QString(), 5}};
    watcher.poll();

    QCOMPARE(prepended.count(), 1);
    QCOMPARE(provider->rowCount(), 1);
    QCOMPARE(provider->newestRecordId(), 5);
}

void TestEventLog::modelPrependUpdatesRows()
{
    EventLogReadResult initial;
    initial.records = {
        {eventXmlWithId(100), QString(), 100},
        {eventXmlWithId(99), QString(), 99},
    };
    auto provider = EventLogProvider::fromResult(
        QStringLiteral("System"), QStringLiteral("System"), QString(), 20000, initial);
    LogTableModel model;
    model.setProvider(provider);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.cellText(0, 0), QStringLiteral("100"));

    const QVector<EventLogRecord> batch = {
        {eventXmlWithId(102), QString(), 102},
        {eventXmlWithId(101), QString(), 101},
    };
    QCOMPARE(provider->prependBatch(batch), 2);
    model.prependRows(2);

    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.cellText(0, 0), QStringLiteral("102"));
    QCOMPARE(model.cellText(1, 0), QStringLiteral("101"));
    QCOMPARE(model.cellText(2, 0), QStringLiteral("100"));
    QCOMPARE(model.cellText(3, 0), QStringLiteral("99"));
}

void TestEventLog::modelPrependWithFilter()
{
    EventLogReadResult initial;
    initial.records = {
        {eventXmlWithId(100, 4), QString(), 100},   // Info
        {eventXmlWithId(99, 4), QString(), 99},      // Info
    };
    auto provider = EventLogProvider::fromResult(
        QStringLiteral("System"), QStringLiteral("System"), QString(), 20000, initial);
    LogTableModel model;
    model.setProvider(provider);

    FilterSpec spec;
    spec.levelMask = 1 << logLevelIndex(LogLevel::Info);
    model.setFilterSpec(spec);
    QCOMPARE(model.rowCount(), 2);

    const QVector<EventLogRecord> batch = {
        {eventXmlWithId(102, 2), QString(), 102},   // Error: hidden by the filter
        {eventXmlWithId(101, 4), QString(), 101},   // Info: visible at the top
    };
    QCOMPARE(provider->prependBatch(batch), 2);
    model.prependRows(2);

    // Only the new Info event is visible; the two existing rows keep their
    // identity after the provider prepended two rows.
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.cellText(0, 0), QStringLiteral("101"));
    QCOMPARE(model.cellText(1, 0), QStringLiteral("100"));
    QCOMPARE(model.cellText(2, 0), QStringLiteral("99"));
    QCOMPARE(model.sourceRow(0), 1);
    QCOMPARE(model.visibleRowOf(0), -1);   // hidden Error row
}

void TestEventLog::readsThroughHelperWithoutUac()
{
    if (!ElevatedEventLog::isSupported())
        QSKIP("The elevated helper seam is Windows only.");

    const QString helper = QDir(QCoreApplication::applicationDirPath())
                               .filePath(QStringLiteral("log-viewer.exe"));
    if (!QFileInfo::exists(helper))
        QSKIP("log-viewer.exe is not available next to the test executable.");

    // Exercise the real pipe protocol and helper mode, only without the UAC
    // prompt (runas itself is covered manually).
    ElevatedEventLog::setProgramOverrideForTesting(helper);
    ElevatedEventLog::setUseRunasForTesting(false);
    QString error;
    const EventLogReadResult result =
        ElevatedEventLog::read(QStringLiteral("Application"), QString(), 5, &error);
    ElevatedEventLog::setUseRunasForTesting(true);
    ElevatedEventLog::setProgramOverrideForTesting(QString());

    QVERIFY2(result.error.isEmpty(), qPrintable(result.error.isEmpty() ? error : result.error));
    QCOMPARE(result.records.size(), 5);
    QVERIFY(result.records.first().recordId > 0);
    QVERIFY(!result.records.first().xml.isEmpty());
}

void TestEventLog::propagatesBackendErrors()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->channelResult.error = QStringLiteral("access denied");

    QString error;
    const auto provider = EventLogProvider::openChannel(backend, QStringLiteral("Security"), 10,
                                                        &error);
    QVERIFY(provider == nullptr);
    QCOMPARE(error, QStringLiteral("access denied"));
}

void TestEventLog::rejectsUnsupportedBackend()
{
    auto backend = std::make_shared<MockEventLogBackend>();
    backend->supported = false;

    QString error;
    QVERIFY(EventLogProvider::openChannel(backend, QStringLiteral("System"), 10, &error) == nullptr);
    QVERIFY(error.isEmpty() == false);

    error.clear();
    QVERIFY(EventLogProvider::openFile(backend, QStringLiteral("archive.evtx"), 10, &error)
            == nullptr);
    QVERIFY(error.isEmpty() == false);
}

void TestEventLog::queriesTheApplicationChannelOnWindows()
{
    const EventLogBackendPtr backend = createEventLogBackend();
    if (!backend->isSupported())
        QSKIP("Native Windows event logs are not supported on this platform.");

    QString error;
    const auto provider =
        EventLogProvider::openChannel(backend, QStringLiteral("Application"), 50, &error);
    if (!provider)
        QSKIP(qPrintable(error));   // restricted CI accounts may not read the channel

    QVERIFY(provider->rowCount() > 0);
    const LogEntry &entry = provider->entryAt(0);
    QVERIFY(entry.time.isValid());
    QVERIFY(!entry.message.isEmpty());
    QVERIFY(!provider->levelCounts().isEmpty());

    // Newest first (REQ-EVT-06).
    QVERIFY(provider->rowCount() > 1);
    const QDateTime newest = provider->entryAt(0).time;
    const QDateTime second = provider->entryAt(1).time;
    QVERIFY(newest.isValid());
    QVERIFY(second.isValid());
    QVERIFY(newest >= second);

    // The incremental XPath used by EventLogWatcher must be accepted by the API.
    const EventLogReadResult delta =
        backend->readChannelSince(QStringLiteral("Application"), provider->newestRecordId(), 5);
    QVERIFY2(delta.error.isEmpty(), qPrintable(delta.error));
    for (const EventLogRecord &record : delta.records)
        QVERIFY(record.recordId > provider->newestRecordId());
}

QTEST_GUILESS_MAIN(TestEventLog)
#include "tst_eventlog.moc"
