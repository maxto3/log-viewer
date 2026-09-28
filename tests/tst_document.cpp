#include "core/LogDocument.h"
#include "core/LogSource.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace lv;

/// Tests for multi file documents (spec.md REQ-FILE-04/05): time interleaving,
/// format consistency checks and the concatenation fallback.
class TestDocument : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void singleFileUsesTheSourceDirectly();
    void mergesByTimestamp();
    void reportsSourcePerEntry();
    void rejectsDifferentFormats();
    void fallsBackToConcatenationWithoutTimestamps();
    void levelCountsCoverAllSources();

private:
    QString writeFile(const QString &name, const QByteArray &content);

    QTemporaryDir m_dir;
};

void TestDocument::init()
{
    QVERIFY(m_dir.isValid());
}

QString TestDocument::writeFile(const QString &name, const QByteArray &content)
{
    const QString path = QDir(m_dir.path()).filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    file.write(content);
    file.close();
    return path;
}

void TestDocument::singleFileUsesTheSourceDirectly()
{
    const QString path = writeFile(QStringLiteral("one.log"),
                                   "2026-09-28 10:00:00 INFO hello\n");
    const LogDocument::OpenResult result = LogDocument::open({path}, QString(), 0, true);
    QVERIFY(result.document != nullptr);
    QVERIFY(result.document->isSingleFile());
    QCOMPARE(result.document->rowCount(), 1);
    QCOMPARE(result.document->documentInfo().fileName, QStringLiteral("one.log"));
    QVERIFY(!result.document->documentInfo().multiFile);
}

void TestDocument::mergesByTimestamp()
{
    const QString first = writeFile(QStringLiteral("a.log"),
                                    "2026-09-28 10:00:00 INFO a1\n"
                                    "2026-09-28 10:00:02 INFO a2\n");
    const QString second = writeFile(QStringLiteral("b.log"),
                                     "2026-09-28 10:00:01 INFO b1\n"
                                     "2026-09-28 10:00:03 INFO b2\n");

    const LogDocument::OpenResult result = LogDocument::open({first, second}, QString(), 0, true);
    QVERIFY(result.document != nullptr);
    QVERIFY(!result.timeMergeDisabled);
    QCOMPARE(result.document->rowCount(), 4);

    QStringList order;
    for (int row = 0; row < result.document->rowCount(); ++row)
        order << result.document->entryAt(row).message;
    QCOMPARE(order, QStringList({QStringLiteral("a1"), QStringLiteral("b1"),
                                 QStringLiteral("a2"), QStringLiteral("b2")}));

    const DocumentInfo info = result.document->documentInfo();
    QVERIFY(info.multiFile);
    QCOMPARE(info.lineCount, 4);
}

void TestDocument::reportsSourcePerEntry()
{
    const QString first = writeFile(QStringLiteral("first.log"),
                                    "2026-09-28 10:00:00 INFO from first\n");
    const QString second = writeFile(QStringLiteral("second.log"),
                                     "2026-09-28 10:00:01 INFO from second\n");

    const LogDocument::OpenResult result = LogDocument::open({first, second}, QString(), 0, true);
    QVERIFY(result.document != nullptr);

    QCOMPARE(result.document->entryAt(0).sourceIndex, 0);
    QCOMPARE(result.document->entryAt(1).sourceIndex, 1);
    QCOMPARE(result.document->sourceName(0), QStringLiteral("first.log"));
    QCOMPARE(result.document->sourceName(1), QStringLiteral("second.log"));
}

void TestDocument::rejectsDifferentFormats()
{
    const QString tracing = writeFile(
        QStringLiteral("tracing.log"),
        "2026-09-28T22:15:43.3035871+08:00  INFO ThreadId(02) crate::module: hello\n");
    const QString json = writeFile(QStringLiteral("json.log"),
                                   "{\"time\":\"2026-09-28T22:15:43Z\",\"level\":\"info\","
                                   "\"msg\":\"hello\"}\n");

    const LogDocument::OpenResult result = LogDocument::open({tracing, json}, QString(), 0, true);
    QVERIFY(result.document == nullptr);
    QVERIFY(result.formatMismatch);
    QVERIFY(!result.error.isEmpty());
}

void TestDocument::fallsBackToConcatenationWithoutTimestamps()
{
    const QString first = writeFile(QStringLiteral("plain1.log"), "first line\nsecond line\n");
    const QString second = writeFile(QStringLiteral("plain2.log"), "third line\n");

    const LogDocument::OpenResult result = LogDocument::open({first, second}, QString(), 0, true);
    QVERIFY(result.document != nullptr);
    QVERIFY(result.timeMergeDisabled);
    QCOMPARE(result.document->rowCount(), 3);

    QStringList order;
    for (int row = 0; row < result.document->rowCount(); ++row)
        order << result.document->entryAt(row).message;
    QCOMPARE(order, QStringList({QStringLiteral("first line"), QStringLiteral("second line"),
                                 QStringLiteral("third line")}));
}

void TestDocument::levelCountsCoverAllSources()
{
    const QString first = writeFile(QStringLiteral("levels1.log"),
                                    "2026-09-28 10:00:00 ERROR boom\n");
    const QString second = writeFile(QStringLiteral("levels2.log"),
                                     "2026-09-28 10:00:01 INFO fine\n"
                                     "2026-09-28 10:00:02 INFO fine again\n");

    const LogDocument::OpenResult result = LogDocument::open({first, second}, QString(), 0, true);
    QVERIFY(result.document != nullptr);

    const QVector<int> counts = result.document->levelCounts();
    QCOMPARE(counts.at(logLevelIndex(LogLevel::Error)), 1);
    QCOMPARE(counts.at(logLevelIndex(LogLevel::Info)), 2);
}

QTEST_MAIN(TestDocument)
#include "tst_document.moc"
