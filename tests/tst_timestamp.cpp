#include "core/TimestampParser.h"

#include <QTest>

using namespace lv;

class TestTimestamp : public QObject
{
    Q_OBJECT

private slots:
    void isoWithOffset();
    void isoWithSevenDigitFraction();
    void isoUtc();
    void spaceSeparated();
    void commaFraction();
    void timeOnlyUsesFallbackDate();
    void syslogUsesFallbackYear();
    void syslogRolloverToPreviousYear();
    void invalidInputs_data();
    void invalidInputs();
    void dateFromFileName();
};

void TestTimestamp::isoWithOffset()
{
    const QDateTime value = parseTimestamp(QStringLiteral("2026-09-28T22:15:43+08:00"));
    QVERIFY(value.isValid());
    QCOMPARE(value.toOffsetFromUtc(8 * 3600).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
             QStringLiteral("2026-09-28 22:15:43"));
}

void TestTimestamp::isoWithSevenDigitFraction()
{
    const QDateTime value = parseTimestamp(QStringLiteral("2026-09-28T22:15:43.3035871+08:00"));
    QVERIFY(value.isValid());
    QCOMPARE(value.toOffsetFromUtc(8 * 3600).time().msec(), 303);
    QCOMPARE(value.toOffsetFromUtc(8 * 3600).time().second(), 43);
}

void TestTimestamp::isoUtc()
{
    const QDateTime value = parseTimestamp(QStringLiteral("2026-09-28T22:15:43.500Z"));
    QVERIFY(value.isValid());
    QCOMPARE(value.toUTC().time().msec(), 500);
    QCOMPARE(value.timeSpec(), Qt::UTC);
}

void TestTimestamp::spaceSeparated()
{
    const QDateTime value = parseTimestamp(QStringLiteral("2026-09-28 22:15:43.303"));
    QVERIFY(value.isValid());
    QCOMPARE(value.date(), QDate(2026, 9, 28));
    QCOMPARE(value.time().msec(), 303);
}

void TestTimestamp::commaFraction()
{
    const QDateTime value = parseTimestamp(QStringLiteral("2026-09-28 22:15:43,303"));
    QVERIFY(value.isValid());
    QCOMPARE(value.time().msec(), 303);
}

void TestTimestamp::timeOnlyUsesFallbackDate()
{
    const QDate fallback(2026, 9, 28);
    const QDateTime value = parseTimestamp(QStringLiteral("22:15:43"), fallback);
    QVERIFY(value.isValid());
    QCOMPARE(value.date(), fallback);
    QCOMPARE(value.time().hour(), 22);
}

void TestTimestamp::syslogUsesFallbackYear()
{
    const QDate fallback(2026, 9, 29);
    const QDateTime value = parseTimestamp(QStringLiteral("Sep 28 22:15:43"), fallback);
    QVERIFY(value.isValid());
    QCOMPARE(value.date(), QDate(2026, 9, 28));
}

void TestTimestamp::syslogRolloverToPreviousYear()
{
    // A December timestamp read in January belongs to the previous year.
    const QDate fallback(2026, 1, 2);
    const QDateTime value = parseTimestamp(QStringLiteral("Dec 31 23:59:59"), fallback);
    QVERIFY(value.isValid());
    QCOMPARE(value.date().year(), 2025);
}

void TestTimestamp::invalidInputs_data()
{
    QTest::addColumn<QString>("text");
    QTest::newRow("empty") << QString();
    QTest::newRow("text") << QStringLiteral("no timestamp here");
    QTest::newRow("bad date") << QStringLiteral("2026-13-45 99:99:99");
    QTest::newRow("partial") << QStringLiteral("2026-09-28T");
}

void TestTimestamp::invalidInputs()
{
    QFETCH(QString, text);
    QVERIFY(!parseTimestamp(text).isValid());
}

void TestTimestamp::dateFromFileName()
{
    QCOMPARE(lv::dateFromFileName(QStringLiteral("sslocal.2026-09-28.log")), QDate(2026, 9, 28));
    QCOMPARE(lv::dateFromFileName(QStringLiteral("app.log")), QDate());
}

QTEST_MAIN(TestTimestamp)
#include "tst_timestamp.moc"
