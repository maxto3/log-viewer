#include "core/DurationFormat.h"

#include <QTest>

using namespace lv;

/// Text formatting of the "Loaded in …" status bar label (spec.md REQ-UI-15):
/// seconds with sub-second precision below one minute, minutes and seconds
/// below one hour, hours/minutes/seconds above - zero-valued leading
/// components are omitted.
class TestDuration : public QObject
{
    Q_OBJECT

private slots:
    void subMinuteValuesKeepDecimals();
    void minuteValuesUseSecondsAndMinutes();
    void hourValuesUseAllComponents();
    void negativeValuesClampToZero();
};

void TestDuration::subMinuteValuesKeepDecimals()
{
    QCOMPARE(formatDuration(0), QStringLiteral("0.00 s"));
    QCOMPARE(formatDuration(120), QStringLiteral("0.12 s"));
    QCOMPARE(formatDuration(350), QStringLiteral("0.35 s"));
    QCOMPARE(formatDuration(1234), QStringLiteral("1.23 s"));
    // Two decimals below ten seconds, one decimal above.
    QCOMPARE(formatDuration(5320), QStringLiteral("5.32 s"));
    QCOMPARE(formatDuration(12340), QStringLiteral("12.3 s"));
    QCOMPARE(formatDuration(59400), QStringLiteral("59.4 s"));
    // Rounds up into the next minute instead of printing "60.0 s".
    QCOMPARE(formatDuration(59960), QStringLiteral("1 min"));
}

void TestDuration::minuteValuesUseSecondsAndMinutes()
{
    QCOMPARE(formatDuration(60000), QStringLiteral("1 min"));
    QCOMPARE(formatDuration(65000), QStringLiteral("1 min 5 s"));
    QCOMPARE(formatDuration(125000), QStringLiteral("2 min 5 s"));
    QCOMPARE(formatDuration(3599000), QStringLiteral("59 min 59 s"));
}

void TestDuration::hourValuesUseAllComponents()
{
    QCOMPARE(formatDuration(3600000), QStringLiteral("1 h"));
    QCOMPARE(formatDuration(3605000), QStringLiteral("1 h 5 s"));
    QCOMPARE(formatDuration(3720000), QStringLiteral("1 h 2 min"));
    QCOMPARE(formatDuration(3725000), QStringLiteral("1 h 2 min 5 s"));
    QCOMPARE(formatDuration(7265000), QStringLiteral("2 h 1 min 5 s"));
    QCOMPARE(formatDuration(86400000), QStringLiteral("24 h"));
}

void TestDuration::negativeValuesClampToZero()
{
    QCOMPARE(formatDuration(-1), QStringLiteral("0.00 s"));
}

QTEST_MAIN(TestDuration)
#include "tst_duration.moc"
