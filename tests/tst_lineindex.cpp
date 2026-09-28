#include "core/LineIndex.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace lv;

class TestLineIndex : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void lfFile();
    void crlfFile();
    void mixedLineEndings();
    void noTrailingNewline();
    void emptyFile();
    void blankLines();
    void respectsLineLimit();
    void longLine();

private:
    QString writeFile(const QByteArray &content);

    QTemporaryDir m_dir;
    QString m_path;
};

void TestLineIndex::init()
{
    QVERIFY(m_dir.isValid());
    m_path = QDir(m_dir.path()).filePath(QStringLiteral("sample.log"));
}

void TestLineIndex::cleanup()
{
    QFile::remove(m_path);
}

QString TestLineIndex::writeFile(const QByteArray &content)
{
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();
    file.write(content);
    file.close();
    return m_path;
}

void TestLineIndex::lfFile()
{
    QVERIFY(!writeFile("one\ntwo\nthree\n").isEmpty());
    LineIndex index;
    QString error;
    QVERIFY2(index.build(m_path, 0, &error), qPrintable(error));
    QCOMPARE(index.lineCount(), 3);
    QCOMPARE(index.lineStart(0), 0);
    QCOMPARE(index.lineLength(0), 3);
    QCOMPARE(index.lineLength(2), 5);
}

void TestLineIndex::crlfFile()
{
    QVERIFY(!writeFile("one\r\ntwo\r\n").isEmpty());
    LineIndex index;
    QVERIFY(index.build(m_path, 0, nullptr));
    QCOMPARE(index.lineCount(), 2);
    QCOMPARE(index.lineLength(0), 3);   // '\r' excluded
    QCOMPARE(index.lineLength(1), 3);
}

void TestLineIndex::mixedLineEndings()
{
    QVERIFY(!writeFile("a\r\nb\nc\r\n").isEmpty());
    LineIndex index;
    QVERIFY(index.build(m_path, 0, nullptr));
    QCOMPARE(index.lineCount(), 3);
    QCOMPARE(index.lineLength(0), 1);
    QCOMPARE(index.lineLength(1), 1);
    QCOMPARE(index.lineLength(2), 1);
}

void TestLineIndex::noTrailingNewline()
{
    QVERIFY(!writeFile("one\ntwo").isEmpty());
    LineIndex index;
    QVERIFY(index.build(m_path, 0, nullptr));
    QCOMPARE(index.lineCount(), 2);
    QCOMPARE(index.lineLength(1), 3);
}

void TestLineIndex::emptyFile()
{
    QVERIFY(!writeFile(QByteArray()).isEmpty());
    LineIndex index;
    QVERIFY(index.build(m_path, 0, nullptr));
    QCOMPARE(index.lineCount(), 0);
    QCOMPARE(index.fileSize(), 0);
}

void TestLineIndex::blankLines()
{
    QVERIFY(!writeFile("\n\nx\n").isEmpty());
    LineIndex index;
    QVERIFY(index.build(m_path, 0, nullptr));
    QCOMPARE(index.lineCount(), 3);
    QCOMPARE(index.lineLength(0), 0);
    QCOMPARE(index.lineLength(1), 0);
    QCOMPARE(index.lineLength(2), 1);
}

void TestLineIndex::respectsLineLimit()
{
    QByteArray content;
    for (int i = 0; i < 100; ++i)
        content += QByteArray::number(i) + "\n";
    QVERIFY(!writeFile(content).isEmpty());

    LineIndex index;
    QVERIFY(index.build(m_path, 10, nullptr));
    QCOMPARE(index.lineCount(), 10);
    QVERIFY(index.isTruncated());
}

void TestLineIndex::longLine()
{
    const QByteArray longLine(1024 * 1024, 'x');
    QVERIFY(!writeFile(longLine + "\nshort\n").isEmpty());
    LineIndex index;
    QVERIFY(index.build(m_path, 0, nullptr));
    QCOMPARE(index.lineCount(), 2);
    QCOMPARE(index.lineLength(0), 1024 * 1024);
    QCOMPARE(index.maxLineLength(), 1024 * 1024);
}

QTEST_MAIN(TestLineIndex)
#include "tst_lineindex.moc"
