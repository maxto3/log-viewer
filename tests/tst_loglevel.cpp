#include "core/LogLevel.h"

#include <QTest>

using namespace lv;

class TestLogLevel : public QObject
{
    Q_OBJECT

private slots:
    void parsesCommonTokens_data();
    void parsesCommonTokens();
    void rejectsNonLevelWords();
    void syslogPriorityMapping();
    void canonicalNames();
};

void TestLogLevel::parsesCommonTokens_data()
{
    QTest::addColumn<QString>("token");
    QTest::addColumn<int>("expected");

    const int trace = static_cast<int>(LogLevel::Trace);
    const int debug = static_cast<int>(LogLevel::Debug);
    const int info = static_cast<int>(LogLevel::Info);
    const int notice = static_cast<int>(LogLevel::Notice);
    const int warn = static_cast<int>(LogLevel::Warn);
    const int error = static_cast<int>(LogLevel::Error);
    const int fatal = static_cast<int>(LogLevel::Fatal);
    const int other = static_cast<int>(LogLevel::Other);

    QTest::newRow("INFO") << QStringLiteral("INFO") << info;
    QTest::newRow("info lowercase") << QStringLiteral("info") << info;
    QTest::newRow("bracketed") << QStringLiteral("[WARN]") << warn;
    QTest::newRow("with colon") << QStringLiteral("ERROR:") << error;
    QTest::newRow("WARNING") << QStringLiteral("WARNING") << warn;
    QTest::newRow("ERR") << QStringLiteral("ERR") << error;
    QTest::newRow("FTL") << QStringLiteral("FTL") << fatal;
    QTest::newRow("TRC") << QStringLiteral("TRC") << trace;
    QTest::newRow("DBG") << QStringLiteral("DBG") << debug;
    QTest::newRow("NOTICE") << QStringLiteral("NOTICE") << notice;
    QTest::newRow("CRITICAL") << QStringLiteral("CRITICAL") << fatal;
    QTest::newRow("empty") << QString() << other;
    QTest::newRow("random") << QStringLiteral("shadowsocks") << other;
}

void TestLogLevel::parsesCommonTokens()
{
    QFETCH(QString, token);
    QFETCH(int, expected);
    QCOMPARE(static_cast<int>(logLevelFromString(token)), expected);
}

void TestLogLevel::rejectsNonLevelWords()
{
    QCOMPARE(logLevelFromString(QStringLiteral("I/O")), LogLevel::Other);
    QCOMPARE(logLevelFromString(QStringLiteral("E-Mail")), LogLevel::Other);
    QCOMPARE(logLevelFromString(QStringLiteral("traceback")), LogLevel::Other);
    // Windows event logs spell it out.
    QCOMPARE(logLevelFromString(QStringLiteral("information")), LogLevel::Info);
}

void TestLogLevel::syslogPriorityMapping()
{
    QCOMPARE(logLevelFromSyslogPriority(0), LogLevel::Fatal);
    QCOMPARE(logLevelFromSyslogPriority(3), LogLevel::Error);
    QCOMPARE(logLevelFromSyslogPriority(4), LogLevel::Warn);
    QCOMPARE(logLevelFromSyslogPriority(5), LogLevel::Notice);
    QCOMPARE(logLevelFromSyslogPriority(6), LogLevel::Info);
    QCOMPARE(logLevelFromSyslogPriority(7), LogLevel::Debug);
    QCOMPARE(logLevelFromSyslogPriority(42), LogLevel::Other);
}

void TestLogLevel::canonicalNames()
{
    QCOMPARE(logLevelCanonicalName(LogLevel::Warn), QStringLiteral("WARN"));
    QCOMPARE(logLevelCanonicalName(LogLevel::Other), QStringLiteral("OTHER"));
    QCOMPARE(logLevelIndex(LogLevel::Error), 5);
    QCOMPARE(logLevelIndex(static_cast<LogLevel>(99)), static_cast<int>(LogLevel::Other));
}

QTEST_MAIN(TestLogLevel)
#include "tst_loglevel.moc"
