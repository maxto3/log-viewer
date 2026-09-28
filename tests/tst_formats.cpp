#include "core/LogFormatRegistry.h"
#include "core/LogSource.h"

#include <QTest>

using namespace lv;

class TestFormats : public QObject
{
    Q_OBJECT

private slots:
    void tracingDetection();
    void tracingFields();
    void tracingIgnoresNonMatchingLines();
    void genericFallbackKeepsWholeLine();
    void registryExposesFormats();
#ifdef LOGVIEWER_SAMPLE_LOG
    void realSampleLog();
#endif
#ifdef LOGVIEWER_FROZEN_SAMPLE
    void frozenSampleSlice();
#endif
};

void TestFormats::tracingDetection()
{
    const QStringList sample = {
        QStringLiteral("2026-09-28T22:15:43.3035871+08:00  INFO ThreadId(02) crate::module: hello"),
        QStringLiteral("2026-09-28T22:15:43.3145572+08:00 DEBUG tokio-rt-worker ThreadId(04) "
                       "crate::other: world"),
    };
    const ILogFormat *format = LogFormatRegistry::instance().detect(sample);
    QVERIFY(format != nullptr);
    QCOMPARE(format->id(), QStringLiteral("tracing"));
}

void TestFormats::tracingFields()
{
    const ILogFormat *format = LogFormatRegistry::instance().findById(QStringLiteral("tracing"));
    QVERIFY(format != nullptr);

    LogEntry entry;
    const QString line = QStringLiteral(
        "2026-09-28T22:15:44.0881187+08:00  INFO tokio-rt-worker ThreadId(27) "
        "shadowsocks_service::local::http::server: shadowsocks HTTP listening on 127.0.0.1:1081");
    QVERIFY(format->parseLine(line, 16, entry));
    QCOMPARE(entry.rawLevel, QStringLiteral("INFO"));
    QCOMPARE(entry.level, LogLevel::Info);
    QCOMPARE(entry.thread, QStringLiteral("tokio-rt-worker"));
    QCOMPARE(entry.target, QStringLiteral("shadowsocks_service::local::http::server"));
    QCOMPARE(entry.message, QStringLiteral("shadowsocks HTTP listening on 127.0.0.1:1081"));
    QCOMPARE(entry.firstLine, 16);
    QCOMPARE(entry.extra.value(QStringLiteral("ThreadId")), QStringLiteral("27"));
    QVERIFY(entry.time.isValid());

    // Lines without a thread name (plain "ThreadId(02)").
    LogEntry second;
    QVERIFY(format->parseLine(
        QStringLiteral("2026-09-28T22:15:43.3035871+08:00  INFO ThreadId(02) "
                       "shadowsocks_rust::service::local: shadowsocks local 1.25.0 build"),
        1, second));
    QCOMPARE(second.target, QStringLiteral("shadowsocks_rust::service::local"));
    QCOMPARE(second.message, QStringLiteral("shadowsocks local 1.25.0 build"));
}

void TestFormats::tracingIgnoresNonMatchingLines()
{
    const ILogFormat *format = LogFormatRegistry::instance().findById(QStringLiteral("tracing"));
    QVERIFY(format != nullptr);
    LogEntry entry;
    QVERIFY(!format->parseLine(QStringLiteral("plain text without a timestamp"), 1, entry));
}

void TestFormats::genericFallbackKeepsWholeLine()
{
    const ILogFormat *format = LogFormatRegistry::instance().findById(QStringLiteral("generic"));
    QVERIFY(format != nullptr);

    LogEntry entry;
    QVERIFY(format->parseLine(QStringLiteral("completely unstructured output"), 7, entry));
    QCOMPARE(entry.message, QStringLiteral("completely unstructured output"));
    QCOMPARE(entry.level, LogLevel::Other);
    QCOMPARE(entry.firstLine, 7);

    LogEntry timestamped;
    QVERIFY(format->parseLine(QStringLiteral("2026-09-28 22:15:43 INFO something happened"), 8,
                              timestamped));
    QVERIFY(timestamped.time.isValid());
    QCOMPARE(timestamped.level, LogLevel::Info);
    QCOMPARE(timestamped.message, QStringLiteral("something happened"));
}

void TestFormats::registryExposesFormats()
{
    const auto formats = LogFormatRegistry::instance().formats();
    QVERIFY(formats.size() >= 2);
    QVERIFY(LogFormatRegistry::instance().findById(QStringLiteral("nope")) == nullptr);
}

#ifdef LOGVIEWER_SAMPLE_LOG
void TestFormats::realSampleLog()
{
    // The repository sample is a *live* log file (the shadowsocks client keeps
    // appending to it), so only structural invariants are asserted here. Exact
    // numbers are covered by the frozen slice below.
    QString error;
    const auto source = LogSource::open(QString::fromUtf8(LOGVIEWER_SAMPLE_LOG), QString(), &error, 0);
    QVERIFY2(source != nullptr, qPrintable(error));

    const DocumentInfo info = source->documentInfo();
    QCOMPARE(info.formatId, QStringLiteral("tracing"));
    QCOMPARE(info.encodingName, QStringLiteral("UTF-8"));
    QVERIFY(info.lineCount > 500);

    const QVector<int> counts = source->levelCounts();
    QCOMPARE(counts.size(), kLogLevelCount);
    int sum = 0;
    for (int count : counts)
        sum += count;
    QCOMPARE(sum, info.lineCount);
    QVERIFY(counts.at(logLevelIndex(LogLevel::Debug)) > 0);
    QVERIFY(counts.at(logLevelIndex(LogLevel::Info)) > 0);
    QVERIFY(counts.at(logLevelIndex(LogLevel::Error)) > 0);

    // Every line parses into a timestamped entry with a message.
    int nonAsciiMessages = 0;
    for (int row = 0; row < info.lineCount; ++row) {
        const LogEntry &entry = source->entryAt(row);
        QVERIFY(entry.time.isValid());
        QVERIFY(!entry.message.isEmpty());
        QVERIFY(!entry.target.isEmpty());
        if (entry.message != QString::fromLatin1(entry.message.toLatin1()))
            ++nonAsciiMessages;
    }

    int longestLine = 0;
    for (int row = 0; row < info.lineCount; ++row)
        longestLine = qMax(longestLine, source->readLine(row).size());

    QVERIFY(nonAsciiMessages > 100);      // Chinese OS error messages
    QVERIFY(longestLine > 200);
}
#endif

#ifdef LOGVIEWER_FROZEN_SAMPLE
void TestFormats::frozenSampleSlice()
{
    // 500 lines frozen from the live sample (tests/data/tracing-sample.log).
    QString error;
    const auto source =
        LogSource::open(QString::fromUtf8(LOGVIEWER_FROZEN_SAMPLE), QString(), &error, 0);
    QVERIFY2(source != nullptr, qPrintable(error));

    const DocumentInfo info = source->documentInfo();
    QCOMPARE(info.formatId, QStringLiteral("tracing"));
    QCOMPARE(info.encodingName, QStringLiteral("UTF-8"));
    QCOMPARE(info.lineCount, 500);

    const QVector<int> counts = source->levelCounts();
    QCOMPARE(counts.at(logLevelIndex(LogLevel::Debug)), 492);
    QCOMPARE(counts.at(logLevelIndex(LogLevel::Info)), 6);
    QCOMPARE(counts.at(logLevelIndex(LogLevel::Error)), 2);
    QCOMPARE(counts.at(logLevelIndex(LogLevel::Warn)), 0);

    int longestLine = 0;
    int nonAsciiLines = 0;
    for (int row = 0; row < info.lineCount; ++row) {
        const QString line = source->readLine(row);
        longestLine = qMax(longestLine, line.size());
        if (line != QString::fromLatin1(line.toLatin1()))
            ++nonAsciiLines;
    }
    QCOMPARE(longestLine, 368);
    QCOMPARE(nonAsciiLines, 20);

    // Spot checks of the parsed fields.
    const LogEntry &first = source->entryAt(0);
    QCOMPARE(first.level, LogLevel::Info);
    QCOMPARE(first.rawLevel, QStringLiteral("INFO"));
    QCOMPARE(first.target, QStringLiteral("shadowsocks_rust::service::local"));
    QCOMPARE(first.message.left(17), QStringLiteral("shadowsocks local"));
    QVERIFY(first.time.isValid());
}
#endif

QTEST_MAIN(TestFormats)
#include "tst_formats.moc"
