#include "core/LogTableModel.h"
#include "core/Matcher.h"
#include "highlight/SnippetTokenizer.h"

#include <QTest>

using namespace lv;

namespace {

/// Small in-memory document used by the filter/find tests.
class TestProvider : public IEntryProvider
{
public:
    TestProvider()
    {
        const QDateTime day(QDate(2026, 9, 28), QTime(10, 0, 0));
        append(LogLevel::Debug, day.addSecs(0), QStringLiteral("started plugin listener"), 8);
        append(LogLevel::Info, day.addSecs(1), QStringLiteral("best TCP server chosen"), 9);
        append(LogLevel::Warn, day.addSecs(2), QStringLiteral("slow tunnel detected"), 10);
        append(LogLevel::Error, day.addSecs(3), QStringLiteral("connection reset"), 11);
        append(LogLevel::Error, day.addSecs(4), QStringLiteral("connection closed"), 12);
    }

    int rowCount() const override { return m_entries.size(); }
    const LogEntry &entryAt(int row) const override { return m_entries.at(row); }

    QVector<int> levelCounts() const override
    {
        QVector<int> counts(kLogLevelCount, 0);
        for (const LogEntry &entry : m_entries)
            counts[logLevelIndex(entry.level)] += 1;
        return counts;
    }

    DocumentInfo documentInfo() const override
    {
        DocumentInfo info;
        info.fileName = QStringLiteral("unit-test.log");
        info.formatName = QStringLiteral("unit test");
        info.encodingName = QStringLiteral("UTF-8");
        info.lineCount = m_entries.size();
        return info;
    }

    QString sourceName(int sourceIndex) const override
    {
        Q_UNUSED(sourceIndex);
        return QStringLiteral("unit-test.log");
    }

    /// Appends a synthetic entry (also used to emulate live monitoring).
    void append(LogLevel level, const QDateTime &time, const QString &message, qint64 line)
    {
        LogEntry entry;
        entry.level = level;
        entry.rawLevel = logLevelCanonicalName(level);
        entry.time = time;
        entry.message = message;
        entry.target = QStringLiteral("unit::test");
        entry.firstLine = line;
        m_entries.append(entry);
    }

private:
    QVector<LogEntry> m_entries;
};

QVector<int> levelBits(std::initializer_list<LogLevel> levels)
{
    int mask = 0;
    for (LogLevel level : levels)
        mask |= 1 << logLevelIndex(level);
    return {mask};
}

} // namespace

class TestFilter : public QObject
{
    Q_OBJECT

private slots:
    void levelFilter();
    void keywordFilter();
    void timeRangeFilter();
    void combinedFilters();
    void clearingTheFilterRestoresAllRows();
    void findRangesAndNavigation();
    void findRespectsTheActiveFilter();
    void appendHonoursTheActiveFilter();
    void snippetTokens();
};

void TestFilter::levelFilter()
{
    auto provider = std::make_shared<TestProvider>();
    LogTableModel model;
    model.setProvider(provider);
    QCOMPARE(model.rowCount(), 5);

    FilterSpec spec;
    spec.levelMask = 1 << logLevelIndex(LogLevel::Error);
    model.setFilterSpec(spec);

    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.isFiltered());
    QCOMPARE(model.sourceRow(0), 3);
    QCOMPARE(model.sourceRow(1), 4);
    QCOMPARE(model.cellText(0, model.columnCount() - 1), QStringLiteral("connection reset"));
    QCOMPARE(model.visibleRowOf(3), 0);
    QCOMPARE(model.visibleRowOf(0), -1);
}

void TestFilter::keywordFilter()
{
    auto provider = std::make_shared<TestProvider>();
    LogTableModel model;
    model.setProvider(provider);

    FilterSpec spec;
    spec.keyword = Matcher::build(QStringLiteral("connection"), MatchMode::WholeWord, true);
    QVERIFY(spec.keyword.isValid());
    model.setFilterSpec(spec);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.cellText(0, model.columnCount() - 1), QStringLiteral("connection reset"));
    QCOMPARE(model.cellText(1, model.columnCount() - 1), QStringLiteral("connection closed"));
}

void TestFilter::timeRangeFilter()
{
    auto provider = std::make_shared<TestProvider>();
    LogTableModel model;
    model.setProvider(provider);

    FilterSpec spec;
    spec.timeRangeActive = true;
    spec.timeFrom = QDateTime(QDate(2026, 9, 28), QTime(10, 0, 2));
    spec.timeTo = QDateTime(QDate(2026, 9, 28), QTime(10, 0, 3));
    model.setFilterSpec(spec);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.cellText(0, model.columnCount() - 1), QStringLiteral("slow tunnel detected"));

    // Entries without a timestamp are hidden by default (REQ-FILTER-08).
    spec.includeInvalidTime = false;
    spec.timeRangeActive = true;
    model.setFilterSpec(spec);
    QCOMPARE(model.rowCount(), 2);
}

void TestFilter::combinedFilters()
{
    auto provider = std::make_shared<TestProvider>();
    LogTableModel model;
    model.setProvider(provider);

    FilterSpec spec;
    spec.levelMask = (1 << logLevelIndex(LogLevel::Error)) | (1 << logLevelIndex(LogLevel::Warn));
    spec.keyword = Matcher::build(QStringLiteral("connection*"), MatchMode::Wildcard, true);
    model.setFilterSpec(spec);

    QCOMPARE(model.rowCount(), 2);   // only the two ERROR lines
    QCOMPARE(model.cellText(0, model.columnCount() - 1), QStringLiteral("connection reset"));
}

void TestFilter::clearingTheFilterRestoresAllRows()
{
    auto provider = std::make_shared<TestProvider>();
    LogTableModel model;
    model.setProvider(provider);

    FilterSpec spec;
    spec.levelMask = 1 << logLevelIndex(LogLevel::Info);
    model.setFilterSpec(spec);
    QCOMPARE(model.rowCount(), 1);

    model.setFilterSpec(FilterSpec());
    QCOMPARE(model.rowCount(), 5);
    QVERIFY(!model.isFiltered());
}

void TestFilter::findRangesAndNavigation()
{
    auto provider = std::make_shared<TestProvider>();
    LogTableModel model;
    model.setProvider(provider);

    model.setFindMatcher(Matcher::build(QStringLiteral("connection"), MatchMode::WholeWord, true));
    const int messageColumn = model.columnCount() - 1;
    QCOMPARE(model.findRanges(0, messageColumn).size(), 0);
    QCOMPARE(model.findRanges(3, messageColumn).size(), 1);

    QCOMPARE(model.findMatchRowCount(), 2);
    QCOMPARE(model.findRowAt(0), 3);
    QCOMPARE(model.findRowAt(1), 4);
    QCOMPARE(model.findOrdinalOf(4), 2);
    QCOMPARE(model.findOrdinalOf(0), -1);
}

void TestFilter::findRespectsTheActiveFilter()
{
    auto provider = std::make_shared<TestProvider>();
    LogTableModel model;
    model.setProvider(provider);

    FilterSpec spec;
    spec.levelMask = 1 << logLevelIndex(LogLevel::Warn);
    model.setFilterSpec(spec);
    model.setFindMatcher(Matcher::build(QStringLiteral("tunnel"), MatchMode::WholeWord, true));

    QCOMPARE(model.findMatchRowCount(), 1);
    QCOMPARE(model.findRowAt(0), 0);    // one visible row, the WARN line
}

void TestFilter::appendHonoursTheActiveFilter()
{
    auto provider = std::make_shared<TestProvider>();
    auto *rawProvider = provider.get();
    LogTableModel model;
    model.setProvider(provider);

    FilterSpec spec;
    spec.levelMask = 1 << logLevelIndex(LogLevel::Error);
    model.setFilterSpec(spec);
    QCOMPARE(model.rowCount(), 2);

    const QDateTime day(QDate(2026, 9, 28), QTime(10, 0, 0));
    rawProvider->append(LogLevel::Info, day.addSecs(5), QStringLiteral("ignored"), 13);
    model.appendRows(5);
    QCOMPARE(model.rowCount(), 2);      // filtered out

    rawProvider->append(LogLevel::Error, day.addSecs(6), QStringLiteral("appended"), 14);
    model.appendRows(6);
    QCOMPARE(model.rowCount(), 3);      // appended rows become visible
    QCOMPARE(model.sourceRow(2), 6);
    QCOMPARE(model.cellText(2, model.columnCount() - 1), QStringLiteral("appended"));
}

void TestFilter::snippetTokens()
{
    auto provider = std::make_shared<TestProvider>();
    LogTableModel model;
    model.setProvider(provider);

    const int messageColumn = model.columnCount() - 1;
    const QString json = QStringLiteral(
        "config = {\"server\":\"1.2.3.4\",\"port\":9684,\"enabled\":true,\"retries\":3}");
    const QVector<TokenSpan> tokens = SnippetTokenizer::tokenize(json);
    QVERIFY(!tokens.isEmpty());

    bool hasKey = false;
    bool hasString = false;
    bool hasNumber = false;
    for (const TokenSpan &span : tokens) {
        if (span.kind == TokenKind::Key)
            hasKey = true;
        if (span.kind == TokenKind::String)
            hasString = true;
        if (span.kind == TokenKind::Number)
            hasNumber = true;
    }
    QVERIFY(hasKey);
    QVERIFY(hasString);
    QVERIFY(hasNumber);

    // The model caches tokens per cell.
    QVERIFY(model.tokenSpans(0, messageColumn).isEmpty());
}

QTEST_MAIN(TestFilter)
#include "tst_filter.moc"
