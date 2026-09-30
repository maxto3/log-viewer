#include "highlight/HighlightTheme.h"
#include "highlight/SnippetTokenizer.h"

#include <QElapsedTimer>
#include <QTest>

using namespace lv;

class TestHighlight : public QObject
{
    Q_OBJECT

private slots:
    void jsonSnippet();
    void jsonIgnoresPlainBraces();
    void jsonHandlesNestedAndEscapedStrings();
    void xmlSnippet();
    void xmlIgnoresComparisonOperators();
    void yamlSnippet();
    void yamlNeedsMultipleLines();
    void snippetsAreCapped();
    void tokenColorsFollowTheTheme();
    void tokenizerIsFastForLongMessages();
};

namespace {

int countKind(const QVector<TokenSpan> &spans, TokenKind kind)
{
    int count = 0;
    for (const TokenSpan &span : spans) {
        if (span.kind == kind)
            ++count;
    }
    return count;
}

} // namespace

void TestHighlight::jsonSnippet()
{
    const QString text =
        QStringLiteral("proxy config = {\"server\":\"23.105.206.76\",\"port\":9684,\"tls\":false}");
    const QVector<TokenSpan> spans = SnippetTokenizer::tokenize(text);
    QVERIFY(countKind(spans, TokenKind::Key) >= 3);
    QVERIFY(countKind(spans, TokenKind::String) >= 1);
    QVERIFY(countKind(spans, TokenKind::Number) >= 1);
    QVERIFY(countKind(spans, TokenKind::Constant) >= 1);

    // Span offsets must stay inside the message.
    for (const TokenSpan &span : spans) {
        QVERIFY(span.start >= 0);
        QVERIFY(span.start + span.length <= text.size());
    }
}

void TestHighlight::jsonIgnoresPlainBraces()
{
    QVERIFY(SnippetTokenizer::tokenize(QStringLiteral("value { }")).isEmpty());
    QVERIFY(SnippetTokenizer::tokenize(QStringLiteral("open { unterminated")).isEmpty());
    QVERIFY(SnippetTokenizer::tokenize(QStringLiteral("ServerStatData { }")).isEmpty());
}

void TestHighlight::jsonHandlesNestedAndEscapedStrings()
{
    const QString text = QStringLiteral(
        R"(data = [{"id":1,"name":"a\"b","nested":{"flag":true}},{"id":2}])");
    const QVector<TokenSpan> spans = SnippetTokenizer::tokenize(text);
    QVERIFY(countKind(spans, TokenKind::Key) >= 4);
    QVERIFY(countKind(spans, TokenKind::Number) >= 2);
    QVERIFY(countKind(spans, TokenKind::Constant) >= 1);

    // The escaped quote must not end the snippet early.
    int lastEnd = 0;
    for (const TokenSpan &span : spans)
        lastEnd = qMax(lastEnd, span.start + span.length);
    QVERIFY(lastEnd > text.size() - 10);
}

void TestHighlight::xmlSnippet()
{
    const QString text = QStringLiteral(
        "socks5 handshake <request version=\"5\"><command>CONNECT</command><host>api.github.com</host></request>");
    const QVector<TokenSpan> spans = SnippetTokenizer::tokenize(text);
    QVERIFY(countKind(spans, TokenKind::TagName) >= 4);
    QVERIFY(countKind(spans, TokenKind::Key) >= 1);              // attribute name
    QVERIFY(countKind(spans, TokenKind::AttributeValue) >= 1);
    QVERIFY(countKind(spans, TokenKind::Punctuation) >= 4);
}

void TestHighlight::xmlIgnoresComparisonOperators()
{
    QVERIFY(SnippetTokenizer::tokenize(QStringLiteral("a < b and c > d")).isEmpty());
    QVERIFY(SnippetTokenizer::tokenize(QStringLiteral("latency < 100ms")).isEmpty());
}

void TestHighlight::yamlSnippet()
{
    const QString text = QStringLiteral(
        "configuration accepted:\n"
        "log:\n"
        "  level: info\n"
        "  timestamps: true\n"
        "servers:\n"
        "  - host: 23.105.206.76\n"
        "    port: 9684\n");
    const QVector<TokenSpan> spans = SnippetTokenizer::tokenize(text);
    QVERIFY(countKind(spans, TokenKind::Key) >= 4);
    QVERIFY(countKind(spans, TokenKind::Constant) >= 1);
    QVERIFY(countKind(spans, TokenKind::Number) >= 1);
}

void TestHighlight::yamlNeedsMultipleLines()
{
    QVERIFY(SnippetTokenizer::tokenize(QStringLiteral("value: 1")).isEmpty());
    QVERIFY(SnippetTokenizer::tokenize(QStringLiteral("http://example.com")).isEmpty());
}

void TestHighlight::snippetsAreCapped()
{
    QString text;
    for (int i = 0; i < 10; ++i)
        text += QStringLiteral("item%1 = {\"a\":%1,\"b\":\"x\"} ").arg(i);
    const QVector<TokenSpan> spans = SnippetTokenizer::tokenize(text);
    QVERIFY(!spans.isEmpty());

    // At most kMaxSnippets snippets: with 3 tokens each the span count stays low.
    QVERIFY(spans.size() <= SnippetTokenizer::kMaxSnippets * 12);
}

void TestHighlight::tokenColorsFollowTheTheme()
{
    const HighlightTheme &dark = HighlightTheme::forDarkMode(true);
    const HighlightTheme &light = HighlightTheme::forDarkMode(false);
    QCOMPARE(dark.name(), QStringLiteral("dark+"));
    QCOMPARE(light.name(), QStringLiteral("light+"));

    QVERIFY(dark.color(TokenKind::String) != light.color(TokenKind::String));
    QCOMPARE(dark.color(TokenKind::Key), QColor(0x9C, 0xDC, 0xFE));
    QCOMPARE(light.color(TokenKind::Key), QColor(0x00, 0x10, 0x80));
    QCOMPARE(dark.snippetBackground(), QColor(0x1E, 0x1E, 0x1E));
}

void TestHighlight::tokenizerIsFastForLongMessages()
{
    // A long message without snippets must not be scanned quadratically.
    QString text;
    for (int i = 0; i < 200; ++i)
        text += QStringLiteral("plain text without any structure %1 ").arg(i);
    QElapsedTimer timer;
    timer.start();
    const QVector<TokenSpan> spans = SnippetTokenizer::tokenize(text);
    const qint64 elapsed = timer.elapsed();
    QVERIFY(spans.isEmpty());
    QVERIFY2(elapsed < 250, qPrintable(QString::number(elapsed) + " ms"));
}

QTEST_MAIN(TestHighlight)
#include "tst_highlight.moc"
