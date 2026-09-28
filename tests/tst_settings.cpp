#include "app/SettingsStore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QVector>

using namespace lv;

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void defaults();
    void stringRoundTrip();
    void fontRoundTrip();
    void themeRoundTrip();
    void detailsPositionRoundTrip();
    void showDetailsPaneRoundTrip();
    void showDetailsPaneMigratesLegacyKeys();
    void recentFilesAreCappedAndDeduped();
    void resetAllRestoresDefaults();

private:
    QString settingsPath(const QString &name) const;

    QTemporaryDir m_dir;
};

void TestSettings::init()
{
    QVERIFY(m_dir.isValid());
}

QString TestSettings::settingsPath(const QString &name) const
{
    return QDir(m_dir.path()).filePath(name + QStringLiteral(".ini"));
}

void TestSettings::defaults()
{
    SettingsStore store(settingsPath(QStringLiteral("defaults")));
    QCOMPARE(store.language(), QStringLiteral("en"));
    QCOMPARE(store.theme(), SettingsStore::Theme::Light);
    QCOMPARE(store.detailsPosition(), SettingsStore::DetailsPosition::Right);
    QCOMPARE(store.highlightBackground().name().toUpper(), QStringLiteral("#7CFC00"));
    QCOMPARE(store.highlightForeground().name().toUpper(), QStringLiteral("#000000"));
    QCOMPARE(store.maxLinesPerFile(), 2000000);
    QCOMPARE(store.rowHeightLines(), 2);
    QVERIFY(store.continuationMerge());
    QVERIFY(!store.tableFont().family().isEmpty());
    QVERIFY(store.headerFont().bold());
    QVERIFY(store.recentFiles().isEmpty());
}

void TestSettings::stringRoundTrip()
{
    const QString path = settingsPath(QStringLiteral("strings"));
    {
        SettingsStore store(path);
        store.setLanguage(QStringLiteral("zh_CN"));
        store.addRecentFile(QStringLiteral("C:/logs/a.log"));
    }
    SettingsStore reopened(path);
    QCOMPARE(reopened.language(), QStringLiteral("zh_CN"));
    QCOMPARE(reopened.recentFiles(), QStringList({QStringLiteral("C:/logs/a.log")}));
}

void TestSettings::fontRoundTrip()
{
    const QString path = settingsPath(QStringLiteral("fonts"));
    QFont font(QStringLiteral("Courier New"), 12);
    {
        SettingsStore store(path);
        store.setTableFont(font);
        store.setHeaderFont(font);
    }
    SettingsStore reopened(path);
    QCOMPARE(reopened.tableFont().family(), QStringLiteral("Courier New"));
    QCOMPARE(reopened.tableFont().pointSize(), 12);
    QVERIFY(reopened.headerFont().bold());
}

void TestSettings::themeRoundTrip()
{
    const QString path = settingsPath(QStringLiteral("theme"));
    {
        SettingsStore store(path);
        store.setTheme(SettingsStore::Theme::Dark);
    }
    SettingsStore reopened(path);
    QCOMPARE(reopened.theme(), SettingsStore::Theme::Dark);
}

void TestSettings::detailsPositionRoundTrip()
{
    const QString path = settingsPath(QStringLiteral("layout"));
    {
        SettingsStore store(path);
        store.setDetailsPosition(SettingsStore::DetailsPosition::Bottom);
    }
    SettingsStore reopened(path);
    QCOMPARE(reopened.detailsPosition(), SettingsStore::DetailsPosition::Bottom);
}

void TestSettings::showDetailsPaneRoundTrip()
{
    const QString path = settingsPath(QStringLiteral("details"));
    {
        SettingsStore store(path);
        QVERIFY(!store.showDetailsPane());
        store.setShowDetailsPane(true);
    }
    SettingsStore reopened(path);
    QVERIFY(reopened.showDetailsPane());
    reopened.setShowDetailsPane(false);
    QVERIFY(!reopened.showDetailsPane());
}

void TestSettings::showDetailsPaneMigratesLegacyKeys()
{
    struct Case {
        QString name;
        QByteArray content;
        bool expected;
    };
    const QVector<Case> cases = {
        {QStringLiteral("legacy-two-options"),
         "[view]\ndetailsOnStartup=true\ndetailsOnRowClick=true\n", true},
        {QStringLiteral("legacy-always-show"), "[view]\nalwaysShowDetails=false\n", false},
        {QStringLiteral("legacy-always-show-on"), "[view]\nalwaysShowDetails=true\n", true},
    };

    for (const Case &testCase : cases) {
        const QString path = settingsPath(testCase.name);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            file.write(testCase.content);
        }
        {
            SettingsStore store(path);
            QCOMPARE(store.showDetailsPane(), testCase.expected);
            store.setShowDetailsPane(testCase.expected);
        }   // the store flushes to disk when it is destroyed

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY2(!content.contains(QStringLiteral("detailsOnStartup")), qPrintable(testCase.name));
        QVERIFY2(!content.contains(QStringLiteral("detailsOnRowClick")), qPrintable(testCase.name));
        QVERIFY2(!content.contains(QStringLiteral("alwaysShowDetails")), qPrintable(testCase.name));
        QVERIFY(content.contains(QStringLiteral("showDetailsPane=")
                                 + (testCase.expected ? QStringLiteral("true")
                                                      : QStringLiteral("false"))));
    }
}

void TestSettings::recentFilesAreCappedAndDeduped()
{
    SettingsStore store(settingsPath(QStringLiteral("recent")));
    for (int i = 0; i < 15; ++i)
        store.addRecentFile(QStringLiteral("file%1.log").arg(i));
    QStringList files = store.recentFiles();
    QCOMPARE(files.size(), 10);
    QCOMPARE(files.first(), QStringLiteral("file14.log"));

    store.addRecentFile(QStringLiteral("file14.log"));
    files = store.recentFiles();
    QCOMPARE(files.size(), 10);
    QCOMPARE(files.count(QStringLiteral("file14.log")), 1);
}

void TestSettings::resetAllRestoresDefaults()
{
    const QString path = settingsPath(QStringLiteral("reset"));
    SettingsStore store(path);
    store.setLanguage(QStringLiteral("zh_CN"));
    store.setTheme(SettingsStore::Theme::Dark);
    store.addRecentFile(QStringLiteral("x.log"));

    store.resetAll();
    QCOMPARE(store.language(), QStringLiteral("en"));
    QCOMPARE(store.theme(), SettingsStore::Theme::Light);
    QVERIFY(store.recentFiles().isEmpty());
}

QTEST_MAIN(TestSettings)
#include "tst_settings.moc"
