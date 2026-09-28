#include "core/LogFormatRegistry.h"
#include "core/LogSource.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace lv;

/// Tests for the formats added in M2 (syslog, structured, application, Windows)
/// plus continuation line merging (REQ-PARSE-06) and block formats.
class TestFormatsExtra : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void syslog3164();    void syslog5424();
    void journalJson();
    void jsonLines();
    void logfmt();
    void csvTable();
    void pythonLogging();
    void serilogConsole();
    void log4jLogback();
    void windowsEventBlocks();
    void windowsEventTsvExport();
    void windowsEventXmlExport();
    void iisW3c();
    void continuationLinesAreMerged();
    void continuationMergingCanBeDisabled();

private:
    QString writeFile(const QString &name, const QByteArray &content);
    std::shared_ptr<LogSource> open(const QString &name, const QByteArray &content,
                                    bool mergeContinuations = true);

    QTemporaryDir m_dir;
};

void TestFormatsExtra::init()
{
    QVERIFY(m_dir.isValid());
}

void TestFormatsExtra::cleanup()
{
    // The temporary directory cleans itself up.
}

QString TestFormatsExtra::writeFile(const QString &name, const QByteArray &content)
{
    const QString path = QDir(m_dir.path()).filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    file.write(content);
    file.close();
    return path;
}

std::shared_ptr<LogSource> TestFormatsExtra::open(const QString &name, const QByteArray &content,
                                                  bool mergeContinuations)
{
    const QString path = writeFile(name, content);
    if (path.isEmpty())
        return nullptr;
    QString error;
    auto source = LogSource::open(path, QString(), &error, 0, mergeContinuations);
    if (!source)
        qWarning("open failed: %s", qPrintable(error));
    return source;
}

void TestFormatsExtra::syslog3164()
{
    const auto source = open(QStringLiteral("syslog.log"),
                             "Sep 28 22:15:43 myhost sshd[1234]: Accepted publickey for user\n"
                             "Sep 28 22:15:44 myhost cron[77]: ERROR: job failed\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("syslog3164"));
    QCOMPARE(source->rowCount(), 2);

    const LogEntry &first = source->entryAt(0);
    QVERIFY(first.time.isValid());
    QCOMPARE(first.host, QStringLiteral("myhost"));
    QCOMPARE(first.target, QStringLiteral("sshd"));
    QCOMPARE(first.pid, QStringLiteral("1234"));
    QCOMPARE(first.message, QStringLiteral("Accepted publickey for user"));

    // The severity inside the message is extracted when present.
    const LogEntry &second = source->entryAt(1);
    QCOMPARE(second.level, LogLevel::Error);
    QCOMPARE(second.message, QStringLiteral("job failed"));
}

void TestFormatsExtra::syslog5424()
{
    const auto source = open(QStringLiteral("syslog5424.log"),
                             "<34>1 2026-09-28T22:15:43.303587+08:00 myhost app 1234 ID47 "
                             "[exampleSDID@32473 iut=\"3\"] application event\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("syslog5424"));
    QCOMPARE(source->rowCount(), 1);

    const LogEntry &entry = source->entryAt(0);
    QVERIFY(entry.time.isValid());
    QCOMPARE(entry.host, QStringLiteral("myhost"));
    QCOMPARE(entry.target, QStringLiteral("app"));
    QCOMPARE(entry.pid, QStringLiteral("1234"));
    QCOMPARE(entry.message, QStringLiteral("application event"));
    QCOMPARE(entry.level, LogLevel::Fatal);          // PRI 34 % 8 = 2 (critical)
    QCOMPARE(entry.extra.value(QStringLiteral("MsgId")), QStringLiteral("ID47"));
}

void TestFormatsExtra::journalJson()
{
    const auto source = open(
        QStringLiteral("journal.json"),
        "{\"__REALTIME_TIMESTAMP\":\"1786504543303587\",\"PRIORITY\":\"6\","
        "\"MESSAGE\":\"Started unit\",\"_HOSTNAME\":\"myhost\","
        "\"SYSLOG_IDENTIFIER\":\"systemd\",\"_PID\":\"1234\"}\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("journal_json"));

    const LogEntry &entry = source->entryAt(0);
    QVERIFY(entry.time.isValid());
    QCOMPARE(entry.level, LogLevel::Info);
    QCOMPARE(entry.message, QStringLiteral("Started unit"));
    QCOMPARE(entry.host, QStringLiteral("myhost"));
    QCOMPARE(entry.target, QStringLiteral("systemd"));
    QCOMPARE(entry.pid, QStringLiteral("1234"));
}

void TestFormatsExtra::jsonLines()
{
    const auto source = open(
        QStringLiteral("app.jsonl"),
        "{\"time\":\"2026-09-28T22:15:43.303Z\",\"level\":\"warn\",\"msg\":\"disk almost full\","
        "\"service\":\"api\",\"retries\":3}\n"
        "{\"@timestamp\":\"2026-09-28T22:15:44.000Z\",\"severity\":\"error\",\"message\":\"boom\","
        "\"nested\":{\"a\":1}}\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("json_lines"));
    QCOMPARE(source->rowCount(), 2);

    const LogEntry &first = source->entryAt(0);
    QVERIFY(first.time.isValid());
    QCOMPARE(first.level, LogLevel::Warn);
    QCOMPARE(first.message, QStringLiteral("disk almost full"));
    QCOMPARE(first.extra.value(QStringLiteral("service")), QStringLiteral("api"));
    QCOMPARE(first.extra.value(QStringLiteral("retries")), QStringLiteral("3"));

    const LogEntry &second = source->entryAt(1);
    QCOMPARE(second.level, LogLevel::Error);
    QCOMPARE(second.message, QStringLiteral("boom"));
    QVERIFY(second.extra.contains(QStringLiteral("nested")));
}

void TestFormatsExtra::logfmt()
{
    const auto source = open(QStringLiteral("app.logfmt"),
                             "time=2026-09-28T22:15:43Z level=error msg=\"connection refused\" "
                             "host=db1 retries=3\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("logfmt"));

    const LogEntry &entry = source->entryAt(0);
    QVERIFY(entry.time.isValid());
    QCOMPARE(entry.level, LogLevel::Error);
    QCOMPARE(entry.message, QStringLiteral("connection refused"));
    QCOMPARE(entry.host, QStringLiteral("db1"));
    QCOMPARE(entry.extra.value(QStringLiteral("retries")), QStringLiteral("3"));
}

void TestFormatsExtra::csvTable()
{
    const auto source = open(QStringLiteral("export.csv"),
                             "Time,Level,Logger,Message,EventId\n"
                             "2026-09-28T22:15:43Z,Error,svc.auth,\"login failed, retry\",42\n"
                             "2026-09-28T22:15:44Z,Information,svc.db,connected,43\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("csv_tsv"));
    QCOMPARE(source->rowCount(), 2);

    const LogEntry &first = source->entryAt(0);
    QVERIFY(first.time.isValid());
    QCOMPARE(first.level, LogLevel::Error);
    QCOMPARE(first.target, QStringLiteral("svc.auth"));
    QCOMPARE(first.message, QStringLiteral("login failed, retry"));   // quoted comma kept
    QCOMPARE(first.extra.value(QStringLiteral("EventId")), QStringLiteral("42"));
}

void TestFormatsExtra::pythonLogging()
{
    const auto source = open(QStringLiteral("python.log"),
                             "2026-09-28 22:15:43,303 - ERROR - my.module - something failed\n"
                             "2026-09-28 22:15:44,000 - INFO - my.module - all good\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("python_logging"));

    const LogEntry &entry = source->entryAt(0);
    QVERIFY(entry.time.isValid());
    QCOMPARE(entry.time.time().msec(), 303);
    QCOMPARE(entry.level, LogLevel::Error);
    QCOMPARE(entry.target, QStringLiteral("my.module"));
    QCOMPARE(entry.message, QStringLiteral("something failed"));
}

void TestFormatsExtra::serilogConsole()
{
    const auto source = open(QStringLiteral("serilog.log"),
                             "2026-09-28 22:15:43.303 +08:00 [ERR] request failed\n"
                             "[22:15:44 INF] worker started\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("serilog"));
    QCOMPARE(source->rowCount(), 2);

    QCOMPARE(source->entryAt(0).level, LogLevel::Error);
    QCOMPARE(source->entryAt(0).message, QStringLiteral("request failed"));
    QVERIFY(source->entryAt(0).time.isValid());

    QCOMPARE(source->entryAt(1).level, LogLevel::Info);
    QCOMPARE(source->entryAt(1).message, QStringLiteral("worker started"));
    QVERIFY(source->entryAt(1).time.isValid());
}

void TestFormatsExtra::log4jLogback()
{
    const auto source = open(QStringLiteral("logback.log"),
                             "2026-09-28 22:15:43,303 [main] INFO  com.example.App - started\n"
                             "2026-09-28 22:15:44,100 [worker-1] WARN  com.example.Job - retrying\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("log4j_logback"));

    const LogEntry &entry = source->entryAt(0);
    QVERIFY(entry.time.isValid());
    QCOMPARE(entry.thread, QStringLiteral("main"));
    QCOMPARE(entry.level, LogLevel::Info);
    QCOMPARE(entry.target, QStringLiteral("com.example.App"));
    QCOMPARE(entry.message, QStringLiteral("started"));
}

void TestFormatsExtra::windowsEventBlocks()
{
    const QByteArray content =
        "TimeCreated : 2026-09-28T22:15:43.3035871+08:00\n"
        "ProviderName : Microsoft-Windows-Kernel-Power\n"
        "EventId : 42\n"
        "LevelDisplayName : Error\n"
        "Message : The system rebooted\n"
        "  because of a power loss\n"
        "TimeCreated : 2026-09-28T22:16:00.0000000+08:00\n"
        "ProviderName : Service Control Manager\n"
        "LevelDisplayName : Information\n"
        "Message : Service started\n";
    const auto source = open(QStringLiteral("events.txt"), content);
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("wevt_text"));
    QCOMPARE(source->rowCount(), 2);

    const LogEntry &first = source->entryAt(0);
    QVERIFY(first.time.isValid());
    QCOMPARE(first.level, LogLevel::Error);
    QCOMPARE(first.target, QStringLiteral("Microsoft-Windows-Kernel-Power"));
    QCOMPARE(first.extra.value(QStringLiteral("EventId")), QStringLiteral("42"));
    QCOMPARE(first.message, QStringLiteral("The system rebooted because of a power loss"));
    QCOMPARE(first.physicalLines, 6);          // five fields + one continuation line
    QCOMPARE(first.firstLine, 1);

    const LogEntry &second = source->entryAt(1);
    QCOMPARE(second.firstLine, 7);
    QCOMPARE(second.level, LogLevel::Info);
    QCOMPARE(second.message, QStringLiteral("Service started"));
}

void TestFormatsExtra::windowsEventTsvExport()
{
    // Event Viewer "Save all events as text" (tab separated, localised headers).
    const QByteArray content =
        QStringLiteral(
            "级别\t日期和时间\t来源\t事件 ID\t任务类别\n"
            "信息\t2026-09-29 1:26:58\tMicrosoft-Windows-Security-SPP\t16384\t无\t安排软件保护服务重新启动\n"
            "错误\t2026-09-29 1:27:01\tApplication Error\t1000\t无\t\"应用程序崩溃\n"
            "更多细节见事件数据\"\n")
            .toUtf8();
    const auto source = open(QStringLiteral("events-tsv.txt"), content);
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("wevt_tsv"));
    QCOMPARE(source->rowCount(), 2);

    const LogEntry &first = source->entryAt(0);
    QVERIFY(first.time.isValid());
    QCOMPARE(first.time.time().hour(), 1);          // single digit hour
    QCOMPARE(first.level, LogLevel::Info);
    QCOMPARE(first.target, QStringLiteral("Microsoft-Windows-Security-SPP"));
    QCOMPARE(first.extra.value(QStringLiteral("Event ID")), QStringLiteral("16384"));
    QVERIFY(!first.extra.contains(QStringLiteral("Task")));   // "无" is dropped
    QCOMPARE(first.message, QStringLiteral("安排软件保护服务重新启动"));
    QCOMPARE(first.firstLine, 1);                    // the header is not a record

    const LogEntry &second = source->entryAt(1);
    QCOMPARE(second.level, LogLevel::Error);
    QCOMPARE(second.physicalLines, 2);               // quoted multi line message
    QCOMPARE(second.message, QStringLiteral("应用程序崩溃\n更多细节见事件数据"));
    QCOMPARE(second.firstLine, 2);                   // numbered records, not lines
}

void TestFormatsExtra::windowsEventXmlExport()
{
    // Event Viewer "Save all events as XML" — several events may share a line.
    const QByteArray content =
        "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\n"
        "<Events>"
        "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'>"
        "<System><Provider Name='Microsoft-Windows-Security-SPP'/>"
        "<EventID>16384</EventID><Level>4</Level>"
        "<TimeCreated SystemTime='2026-09-29T01:26:58.3035871Z'/>"
        "<Computer>DESKTOP-TEST</Computer></System>"
        "<RenderingInfo><Message>software protection check finished</Message></RenderingInfo>"
        "</Event>"
        "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'>"
        "<System><Provider Name='Application Error'/>"
        "<EventID>1000</EventID><Level>2</Level>"
        "<TimeCreated SystemTime='2026-09-29T01:27:01.0000000Z'/>"
        "<Computer>DESKTOP-TEST</Computer></System>"
        "<EventData><Data Name='AppName'>demo.exe</Data><Data>0xc0000005</Data></EventData>"
        "</Event>"
        "</Events>\n";
    const auto source = open(QStringLiteral("events.xml"), content);
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("wevt_xml"));
    QCOMPARE(source->rowCount(), 2);                 // one entry per <Event>

    const LogEntry &first = source->entryAt(0);
    QVERIFY(first.time.isValid());
    QCOMPARE(first.level, LogLevel::Info);           // Level 4
    QCOMPARE(first.target, QStringLiteral("Microsoft-Windows-Security-SPP"));
    QCOMPARE(first.extra.value(QStringLiteral("Event ID")), QStringLiteral("16384"));
    QCOMPARE(first.message, QStringLiteral("software protection check finished"));
    QCOMPARE(first.firstLine, 1);                    // records are numbered: both
                                                     // events share physical line 2

    const LogEntry &second = source->entryAt(1);
    QCOMPARE(second.level, LogLevel::Error);         // Level 2
    QCOMPARE(second.target, QStringLiteral("Application Error"));
    QVERIFY(second.message.contains(QStringLiteral("demo.exe")));
    QCOMPARE(second.firstLine, 2);
}

void TestFormatsExtra::iisW3c()
{
    const auto source = open(QStringLiteral("iis.log"),
                             "#Software: Microsoft Internet Information Services 10.0\n"
                             "#Fields: date time s-ip cs-method cs-uri-stem sc-status\n"
                             "2026-09-28 22:15:43 10.0.0.1 GET /index.html 200\n"
                             "2026-09-28 22:15:44 10.0.0.1 POST /api/login 500\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->format()->id(), QStringLiteral("iis_w3c"));
    QCOMPARE(source->rowCount(), 2);

    const LogEntry &first = source->entryAt(0);
    QVERIFY(first.time.isValid());
    QCOMPARE(first.host, QStringLiteral("10.0.0.1"));
    QCOMPARE(first.level, LogLevel::Info);
    QCOMPARE(first.message, QStringLiteral("GET /index.html 200"));

    QCOMPARE(source->entryAt(1).level, LogLevel::Error);   // 500
}

void TestFormatsExtra::continuationLinesAreMerged()
{
    const auto source = open(QStringLiteral("stack.log"),
                             "2026-09-28 10:00:00 ERROR failed to start\n"
                             "    at com.example.App.main(App.java:42)\n"
                             "    at com.example.App.run(App.java:10)\n"
                             "2026-09-28 10:00:01 INFO recovered\n");
    QVERIFY(source != nullptr);
    QCOMPARE(source->rowCount(), 2);

    const LogEntry &first = source->entryAt(0);
    QCOMPARE(first.physicalLines, 3);
    QCOMPARE(first.message,
             QStringLiteral("failed to start\n"
                            "    at com.example.App.main(App.java:42)\n"
                            "    at com.example.App.run(App.java:10)"));
    QCOMPARE(first.firstLine, 1);
    QCOMPARE(source->entryAt(1).firstLine, 4);
}

void TestFormatsExtra::continuationMergingCanBeDisabled()
{
    const auto source = open(QStringLiteral("stack2.log"),
                             "2026-09-28 10:00:00 ERROR failed to start\n"
                             "    at com.example.App.main(App.java:42)\n"
                             "2026-09-28 10:00:01 INFO recovered\n",
                             false);
    QVERIFY(source != nullptr);
    QCOMPARE(source->rowCount(), 3);
    QCOMPARE(source->entryAt(1).message, QStringLiteral("    at com.example.App.main(App.java:42)"));
}

QTEST_MAIN(TestFormatsExtra)
#include "tst_formats_extra.moc"
