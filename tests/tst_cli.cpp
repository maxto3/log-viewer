#include "app/CliParser.h"

#include <QTest>

using namespace lv;

class TestCliParser : public QObject
{
    Q_OBJECT

private slots:
    void emptyArguments();
    void positionalFiles();
    void helpAndVersion();
    void languageForms_data();
    void languageForms();
    void formatOptions();
    void monitorAndDemo();
    void associationOptions();
    void unknownOption();
    void missingValue();
    void unsupportedLanguage();
};

void TestCliParser::emptyArguments()
{
    const CliOptions options = CliParser::parse({});
    QVERIFY(options.error.isEmpty());
    QVERIFY(options.files.isEmpty());
    QCOMPARE(options.formatId, QStringLiteral("auto"));
    QVERIFY(!options.monitor);
    QVERIFY(!options.demo);
}

void TestCliParser::positionalFiles()
{
    const CliOptions options = CliParser::parse({QStringLiteral("a.log"), QStringLiteral("b.log")});
    QCOMPARE(options.files, QStringList({QStringLiteral("a.log"), QStringLiteral("b.log")}));
    QVERIFY(options.error.isEmpty());
}

void TestCliParser::helpAndVersion()
{
    QVERIFY(CliParser::parse({QStringLiteral("--help")}).helpRequested);
    QVERIFY(CliParser::parse({QStringLiteral("-h")}).helpRequested);
    QVERIFY(CliParser::parse({QStringLiteral("--version")}).versionRequested);
    QVERIFY(CliParser::parse({QStringLiteral("-v")}).versionRequested);
    QVERIFY(!CliParser::usageText().isEmpty());
    QVERIFY(CliParser::versionText().contains(QStringLiteral("log-viewer")));
}

void TestCliParser::languageForms_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("expected");

    QTest::newRow("separate") << QStringList{QStringLiteral("--lang"), QStringLiteral("zh_CN")}
                              << QStringLiteral("zh_CN");
    QTest::newRow("equals") << QStringList{QStringLiteral("--lang=zh_CN")} << QStringLiteral("zh_CN");
    QTest::newRow("english") << QStringList{QStringLiteral("--lang"), QStringLiteral("en")}
                             << QStringLiteral("en");
}

void TestCliParser::languageForms()
{
    QFETCH(QStringList, arguments);
    QFETCH(QString, expected);
    const CliOptions options = CliParser::parse(arguments);
    QVERIFY(options.error.isEmpty());
    QCOMPARE(options.language, expected);
}

void TestCliParser::formatOptions()
{
    CliOptions options = CliParser::parse({QStringLiteral("--format"), QStringLiteral("tracing")});
    QCOMPARE(options.formatId, QStringLiteral("tracing"));
    QVERIFY(!options.listFormats);

    options = CliParser::parse({QStringLiteral("--format=list")});
    QVERIFY(options.listFormats);
    QVERIFY(CliParser::formatListText().contains(QStringLiteral("generic")));
    QVERIFY(CliParser::formatListText().contains(QStringLiteral("tracing")));
}

void TestCliParser::monitorAndDemo()
{
    const CliOptions options = CliParser::parse(
        {QStringLiteral("--monitor"), QStringLiteral("--demo"), QStringLiteral("x.log")});
    QVERIFY(options.monitor);
    QVERIFY(options.demo);
    QCOMPARE(options.files.size(), 1);
}

void TestCliParser::associationOptions()
{
    CliOptions options = CliParser::parse({QStringLiteral("--register-association")});
    QVERIFY(options.error.isEmpty());
    QVERIFY(options.registerAssociation);
    QVERIFY(!options.unregisterAssociation);
    QVERIFY(!options.forceAssociation);

    options = CliParser::parse({QStringLiteral("--unregister-association")});
    QVERIFY(options.error.isEmpty());
    QVERIFY(options.unregisterAssociation);

    options = CliParser::parse({QStringLiteral("--register-association"),
                                QStringLiteral("--force"), QStringLiteral("--lang"),
                                QStringLiteral("zh_CN")});
    QVERIFY(options.error.isEmpty());
    QVERIFY(options.forceAssociation);
    QCOMPARE(options.language, QStringLiteral("zh_CN"));

    // Combinations that make no sense are usage errors (REQ-CLI-10).
    QVERIFY(!CliParser::parse({QStringLiteral("--register-association"),
                               QStringLiteral("--unregister-association")})
                 .error.isEmpty());
    QVERIFY(!CliParser::parse({QStringLiteral("--force")}).error.isEmpty());
    QVERIFY(!CliParser::parse({QStringLiteral("--register-association"), QStringLiteral("a.log")})
                 .error.isEmpty());

    // Documented in --help and in the confirmation the CLI prints.
    QVERIFY(CliParser::usageText().contains(QStringLiteral("--register-association")));
    QVERIFY(CliParser::usageText().contains(QStringLiteral("--unregister-association")));
    QVERIFY(CliParser::associationRegisteredText(QStringLiteral(".log"),
                                                 QStringLiteral("C:\\log-viewer\\log-viewer.exe"))
                .contains(QStringLiteral("log-viewer.exe")));
    QVERIFY(CliParser::associationRemovedText(QStringLiteral(".log"))
                .contains(QStringLiteral(".log")));
}

void TestCliParser::unknownOption()
{
    const CliOptions options = CliParser::parse({QStringLiteral("--nope")});
    QVERIFY(!options.error.isEmpty());
}

void TestCliParser::missingValue()
{
    QVERIFY(!CliParser::parse({QStringLiteral("--lang")}).error.isEmpty());
    QVERIFY(!CliParser::parse({QStringLiteral("--format")}).error.isEmpty());
}

void TestCliParser::unsupportedLanguage()
{
    const CliOptions options = CliParser::parse({QStringLiteral("--lang=de")});
    QVERIFY(!options.error.isEmpty());
}

QTEST_MAIN(TestCliParser)
#include "tst_cli.moc"
