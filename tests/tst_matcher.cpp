#include "core/Matcher.h"

#include <QTest>

using namespace lv;

class TestMatcher : public QObject
{
    Q_OBJECT

private slots:
    void emptyPatternMatchesNothing();
    void wholeWordPlainText();
    void wholeWordBoundaries();
    void wholeWordCjk();
    void wildcard();
    void regularExpression();
    void caseSensitivity();
    void invalidPatternIsReported();
    void patternLengthIsCapped();
    void rangesAreReported();
};

void TestMatcher::emptyPatternMatchesNothing()
{
    const Matcher matcher = Matcher::build(QString(), MatchMode::WholeWord, true);
    QVERIFY(matcher.isEmpty());
    QVERIFY(matcher.isValid());
    QVERIFY(!matcher.matches(QStringLiteral("anything")));
}

void TestMatcher::wholeWordPlainText()
{
    const Matcher matcher = Matcher::build(QStringLiteral("error"), MatchMode::WholeWord, false);
    QVERIFY(matcher.isValid());
    QVERIFY(matcher.matches(QStringLiteral("an ERROR occurred")));
    QVERIFY(!matcher.matches(QStringLiteral("no match here")));
}

void TestMatcher::wholeWordBoundaries()
{
    // "ERROR" must not match inside "ERRORS" or "XERROR".
    const Matcher matcher = Matcher::build(QStringLiteral("ERROR"), MatchMode::WholeWord, true);
    QVERIFY(matcher.matches(QStringLiteral("an ERROR occurred")));
    QVERIFY(!matcher.matches(QStringLiteral("ERRORS occurred")));
    QVERIFY(!matcher.matches(QStringLiteral("XERROR occurred")));
    // Regular expression metacharacters are treated literally.
    const Matcher literal = Matcher::build(QStringLiteral("a.b"), MatchMode::WholeWord, true);
    QVERIFY(literal.matches(QStringLiteral("x a.b y")));
    QVERIFY(!literal.matches(QStringLiteral("x axb y")));
}

void TestMatcher::wholeWordCjk()
{
    // A Chinese pattern must match inside a Chinese sentence (REQ-FIND-08).
    const Matcher matcher = Matcher::build(QStringLiteral("连接"), MatchMode::WholeWord, true);
    QVERIFY(matcher.matches(QStringLiteral("远程主机强迫关闭了一个现有的连接。")));
    QVERIFY(matcher.matches(QStringLiteral("连接")));
    QVERIFY(!matcher.matches(QStringLiteral("已断开")));
}

void TestMatcher::wildcard()
{
    const Matcher matcher = Matcher::build(QStringLiteral("conn*tion"), MatchMode::Wildcard, false);
    QVERIFY(matcher.isValid());
    QVERIFY(matcher.matches(QStringLiteral("CONNECTION closed")));
    QVERIFY(matcher.matches(QStringLiteral("conntion")));
    QVERIFY(!matcher.matches(QStringLiteral("connect")));

    // Case sensitive wildcard search.
    const Matcher sensitive = Matcher::build(QStringLiteral("conn*tion"), MatchMode::Wildcard, true);
    QVERIFY(sensitive.matches(QStringLiteral("connection closed")));
    QVERIFY(!sensitive.matches(QStringLiteral("CONNECTION closed")));

    const Matcher question = Matcher::build(QStringLiteral("por?"), MatchMode::Wildcard, true);
    QVERIFY(question.matches(QStringLiteral("port 443")));
}

void TestMatcher::regularExpression()
{
    const Matcher matcher = Matcher::build(QStringLiteral(R"(ThreadId\(\d+\))"),
                                           MatchMode::RegularExpression, true);
    QVERIFY(matcher.isValid());
    QVERIFY(matcher.matches(QStringLiteral("tokio-rt-worker ThreadId(27) target: msg")));
    QVERIFY(!matcher.matches(QStringLiteral("ThreadId(oops)")));
}

void TestMatcher::caseSensitivity()
{
    const Matcher insensitive = Matcher::build(QStringLiteral("warning"), MatchMode::WholeWord, false);
    QVERIFY(insensitive.matches(QStringLiteral("WARNING: disk full")));

    const Matcher sensitive = Matcher::build(QStringLiteral("warning"), MatchMode::WholeWord, true);
    QVERIFY(!sensitive.matches(QStringLiteral("WARNING: disk full")));
    QVERIFY(sensitive.matches(QStringLiteral("warning: disk full")));
}

void TestMatcher::invalidPatternIsReported()
{
    QString error;
    const Matcher matcher = Matcher::build(QStringLiteral("([unclosed"), MatchMode::RegularExpression,
                                           true, &error);
    QVERIFY(!matcher.isValid());
    QVERIFY(!error.isEmpty());
    QVERIFY(!matcher.matches(QStringLiteral("anything")));
}

void TestMatcher::patternLengthIsCapped()
{
    QString error;
    const QString tooLong(Matcher::kMaxPatternLength + 1, QLatin1Char('a'));
    const Matcher matcher = Matcher::build(tooLong, MatchMode::WholeWord, true, &error);
    QVERIFY(!matcher.isValid());
    QVERIFY(!error.isEmpty());
}

void TestMatcher::rangesAreReported()
{
    const Matcher matcher = Matcher::build(QStringLiteral("ab"), MatchMode::WholeWord, true);
    const QVector<MatchRange> ranges = matcher.ranges(QStringLiteral("ab cd ab"));
    QCOMPARE(ranges.size(), 2);
    QCOMPARE(ranges.at(0).start, 0);
    QCOMPARE(ranges.at(0).length, 2);
    QCOMPARE(ranges.at(1).start, 6);
}

QTEST_MAIN(TestMatcher)
#include "tst_matcher.moc"
