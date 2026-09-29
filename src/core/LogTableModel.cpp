#include "core/LogTableModel.h"

#include "highlight/SnippetTokenizer.h"

#include <QCoreApplication>
#include <QHash>

namespace lv {
namespace {

constexpr int kScanLimit = 200;         ///< entries inspected to build the columns
constexpr int kVisibleRatio = 50;       ///< % of entries required for optional columns
constexpr int kMaxCachedRanges = 4096;  ///< per-cache entry limit

QString columnTitle(ColumnKind kind)
{
    switch (kind) {
    case ColumnKind::Time:    return QCoreApplication::translate("Columns", "Time");
    case ColumnKind::Level:   return QCoreApplication::translate("Columns", "Level");
    case ColumnKind::Thread:  return QCoreApplication::translate("Columns", "Thread");
    case ColumnKind::Target:  return QCoreApplication::translate("Columns", "Target");
    case ColumnKind::Pid:     return QCoreApplication::translate("Columns", "PID");
    case ColumnKind::Host:    return QCoreApplication::translate("Columns", "Host");
    case ColumnKind::File:    return QCoreApplication::translate("Columns", "File");
    case ColumnKind::Line:    return QCoreApplication::translate("Columns", "Line");
    case ColumnKind::Message: return QCoreApplication::translate("Columns", "Message");
    case ColumnKind::Extra:   break;
    }
    return QString();
}

int defaultWidth(ColumnKind kind)
{
    switch (kind) {
    case ColumnKind::Time:    return 230;
    case ColumnKind::Level:   return 78;
    case ColumnKind::Thread:  return 130;
    case ColumnKind::Target:  return 220;
    case ColumnKind::Pid:     return 70;
    case ColumnKind::Host:    return 140;
    case ColumnKind::File:    return 170;
    case ColumnKind::Line:    return 58;
    case ColumnKind::Extra:   return 96;
    case ColumnKind::Message: break;
    }
    return 120;
}

} // namespace

LogTableModel::LogTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void LogTableModel::setProvider(EntryProviderPtr provider)
{
    beginResetModel();
    m_provider = std::move(provider);
    rebuildColumns();
    clearRowCaches();
    rebuildFilter();
    endResetModel();
}

void LogTableModel::rebuildColumns()
{
    // The titles are not stored: headerData() translates them on demand, so a
    // language switch updates the table header without rebuilding the columns
    // (REQ-I18N-02, see retranslateHeaders()).
    m_columns.clear();
    if (!m_provider || m_provider->rowCount() <= 0) {
        m_columns.append({ColumnKind::Line, {}, defaultWidth(ColumnKind::Line)});
        m_columns.append({ColumnKind::Time, {}, defaultWidth(ColumnKind::Time)});
        m_columns.append({ColumnKind::Level, {}, defaultWidth(ColumnKind::Level)});
        m_columns.append({ColumnKind::Message, {}, 0});
        return;
    }

    const int rows = m_provider->rowCount();
    const int scan = qMin(kScanLimit, rows);

    int withTime = 0;
    int withLevel = 0;
    QHash<QString, int> threadHits;
    QHash<QString, int> targetHits;
    QHash<QString, int> pidHits;
    QHash<QString, int> hostHits;
    QHash<QString, int> extraHits;

    for (int row = 0; row < scan; ++row) {
        const LogEntry &entry = m_provider->entryAt(row);
        if (entry.time.isValid())
            ++withTime;
        if (entry.level != LogLevel::Other || !entry.rawLevel.isEmpty())
            ++withLevel;
        if (!entry.thread.isEmpty())
            threadHits[QStringLiteral("_")] += 1;
        if (!entry.target.isEmpty())
            targetHits[QStringLiteral("_")] += 1;
        if (!entry.pid.isEmpty())
            pidHits[QStringLiteral("_")] += 1;
        if (!entry.host.isEmpty())
            hostHits[QStringLiteral("_")] += 1;
        for (auto it = entry.extra.constBegin(); it != entry.extra.constEnd(); ++it) {
            if (!it.value().isEmpty())
                ++extraHits[it.key()];
        }
    }

    const auto visible = [scan](int hits) { return hits * 100 >= scan * kVisibleRatio; };

    const DocumentInfo info = m_provider->documentInfo();

    // The physical line number comes first: it is the natural anchor when
    // comparing the table with the raw file (spec.md REQ-PARSE-03).
    m_columns.append({ColumnKind::Line, {}, defaultWidth(ColumnKind::Line)});
    if (info.multiFile)
        m_columns.append({ColumnKind::File, {}, defaultWidth(ColumnKind::File)});
    if (withTime > 0)
        m_columns.append({ColumnKind::Time, {}, defaultWidth(ColumnKind::Time)});
    if (withLevel > 0)
        m_columns.append({ColumnKind::Level, {}, defaultWidth(ColumnKind::Level)});
    if (visible(threadHits.value(QStringLiteral("_"))))
        m_columns.append({ColumnKind::Thread, {}, defaultWidth(ColumnKind::Thread)});
    if (visible(pidHits.value(QStringLiteral("_"))))
        m_columns.append({ColumnKind::Pid, {}, defaultWidth(ColumnKind::Pid)});
    if (visible(hostHits.value(QStringLiteral("_"))))
        m_columns.append({ColumnKind::Host, {}, defaultWidth(ColumnKind::Host)});
    if (visible(targetHits.value(QStringLiteral("_"))))
        m_columns.append({ColumnKind::Target, {}, defaultWidth(ColumnKind::Target)});

    // Dynamic extra columns, ordered alphabetically for stable layouts.
    for (auto it = extraHits.constBegin(); it != extraHits.constEnd(); ++it) {
        if (visible(it.value()))
            m_columns.append({ColumnKind::Extra, it.key(), defaultWidth(ColumnKind::Extra)});
    }

    m_columns.append({ColumnKind::Message, {}, 0});
}

int LogTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_provider)
        return 0;
    if (m_filterActive)
        return static_cast<int>(m_visibleRows.size());
    return m_provider->rowCount();
}

int LogTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_columns.size();
}

ColumnKind LogTableModel::columnKind(int column) const
{
    if (column < 0 || column >= m_columns.size())
        return ColumnKind::Message;
    return m_columns.at(column).kind;
}

QString LogTableModel::extraKey(int column) const
{
    if (column < 0 || column >= m_columns.size())
        return QString();
    return m_columns.at(column).extraKey;
}

int LogTableModel::preferredColumnWidth(int column) const
{
    if (column < 0 || column >= m_columns.size())
        return 120;
    return m_columns.at(column).preferredWidth;
}

QString LogTableModel::columnKey(int column) const
{
    if (column < 0 || column >= m_columns.size())
        return QString();
    const Column &info = m_columns.at(column);
    if (info.kind == ColumnKind::Extra)
        return QStringLiteral("extra:") + info.extraKey;
    return columnKindKey(info.kind);
}

bool LogTableModel::isColumnHidden(int column) const
{
    const QString key = columnKey(column);
    return !key.isEmpty() && m_hiddenColumns.contains(key);
}

void LogTableModel::setColumnHidden(int column, bool hidden)
{
    // The Line column is the anchor for comparing the table with the raw file and
    // is always shown (REQ-PARSE-03), so it cannot be switched off.
    if (hidden && columnKind(column) == ColumnKind::Line)
        return;
    const QString key = columnKey(column);
    if (key.isEmpty())
        return;

    if (hidden) {
        if (m_hiddenColumns.contains(key))
            return;
        m_hiddenColumns.insert(key);
    } else if (m_hiddenColumns.remove(key) == 0) {
        return;
    }

    m_findRowsValid = false;    // Find only searches visible columns
    emit columnsVisibilityChanged();
}

void LogTableModel::setHiddenColumnKeys(const QSet<QString> &keys)
{
    if (m_hiddenColumns == keys)
        return;
    m_hiddenColumns = keys;
    m_findRowsValid = false;
    emit columnsVisibilityChanged();
}

int LogTableModel::sourceRow(int row) const
{
    if (!m_filterActive)
        return row;
    if (row < 0 || row >= static_cast<int>(m_visibleRows.size()))
        return -1;
    return m_visibleRows[static_cast<size_t>(row)];
}

int LogTableModel::visibleRowOf(int sourceRow) const
{
    if (!m_filterActive)
        return sourceRow;
    for (size_t i = 0; i < m_visibleRows.size(); ++i) {
        if (m_visibleRows[i] == sourceRow)
            return static_cast<int>(i);
    }
    return -1;
}

const LogEntry *LogTableModel::entry(int row) const
{
    if (!m_provider)
        return nullptr;
    const int source = sourceRow(row);
    if (source < 0 || source >= m_provider->rowCount())
        return nullptr;
    return &m_provider->entryAt(source);
}

QString LogTableModel::cellText(int row, int column) const
{
    const LogEntry *item = entry(row);
    if (!item)
        return QString();
    return textForEntry(*item, column);
}

QString LogTableModel::textForEntry(const LogEntry &item, int column) const
{
    switch (columnKind(column)) {
    case ColumnKind::Time:
        return item.time.isValid()
            ? item.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
            : QString();
    case ColumnKind::Level:
        return item.rawLevel.isEmpty() ? logLevelCanonicalName(item.level) : item.rawLevel;
    case ColumnKind::Thread:
        return item.thread;
    case ColumnKind::Target:
        return item.target;
    case ColumnKind::Pid:
        return item.pid;
    case ColumnKind::Host:
        return item.host;
    case ColumnKind::File:
        return m_provider ? m_provider->sourceName(item.sourceIndex) : QString();
    case ColumnKind::Line:
        return QString::number(item.firstLine);
    case ColumnKind::Extra:
        return item.extra.value(extraKey(column));
    case ColumnKind::Message:
        break;
    }
    return item.message;
}

QVariant LogTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    switch (role) {
    case Qt::DisplayRole:
    case FullTextRole:
        return cellText(index.row(), index.column());
    case ColumnKindRole:
        return static_cast<int>(columnKind(index.column()));
    case Qt::TextAlignmentRole:
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::ToolTipRole:
        return {};
    default:
        break;
    }
    return {};
}

QVariant LogTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Vertical)
        return {};
    if (section < 0 || section >= m_columns.size())
        return {};
    const Column &column = m_columns.at(section);
    // Translated on demand: a language switch must update the table header
    // without rebuilding the columns (REQ-I18N-02). The key of an extra column
    // is log data, not UI text, so it stays untranslated.
    if (column.kind == ColumnKind::Extra)
        return column.extraKey;
    return columnTitle(column.kind);
}

void LogTableModel::retranslateHeaders()
{
    if (m_columns.isEmpty())
        return;
    // The titles are computed by headerData(); this makes the views repaint the
    // header sections after the translator changed (REQ-I18N-02).
    emit headerDataChanged(Qt::Horizontal, 0, m_columns.size() - 1);
}

void LogTableModel::appendRows(int firstRow)
{
    if (!m_provider)
        return;

    if (!m_filterActive) {
        const int rows = m_provider->rowCount();
        if (firstRow < 0 || firstRow >= rows)
            return;
        beginInsertRows(QModelIndex(), firstRow, rows - 1);
        endInsertRows();
        m_findRowsValid = false;
        return;
    }

    // Filtered: only the appended rows that pass the filter become visible, and
    // they are appended at the end of the visible order.
    const int rows = m_provider->rowCount();
    if (firstRow < 0 || firstRow >= rows)
        return;
    const int visibleBefore = static_cast<int>(m_visibleRows.size());
    for (int source = firstRow; source < rows; ++source) {
        if (entryMatches(m_provider->entryAt(source)))
            m_visibleRows.push_back(source);
    }
    const int added = static_cast<int>(m_visibleRows.size()) - visibleBefore;
    if (added > 0) {
        beginInsertRows(QModelIndex(), visibleBefore, visibleBefore + added - 1);
        endInsertRows();
    }
    m_findRowsValid = false;
}

void LogTableModel::setFilterSpec(const FilterSpec &spec)
{
    m_filter = spec;
    beginResetModel();
    rebuildFilter();
    endResetModel();
}

bool LogTableModel::entryMatches(const LogEntry &item) const
{
    if (m_filter.levelMask != 0) {
        const int bit = 1 << logLevelIndex(item.level);
        if ((m_filter.levelMask & bit) == 0)
            return false;
    }

    if (m_filter.timeRangeActive) {
        if (!item.time.isValid()) {
            if (!m_filter.includeInvalidTime)
                return false;
        } else {
            if (m_filter.timeFrom.isValid() && item.time < m_filter.timeFrom)
                return false;
            if (m_filter.timeTo.isValid() && item.time > m_filter.timeTo)
                return false;
        }
    }

    if (!m_filter.keyword.isEmpty()) {
        // The keyword filter searches every displayed column (REQ-FILTER-05);
        // columns hidden through the Columns menu are not searched.
        bool matched = false;
        for (int column = 0; column < m_columns.size(); ++column) {
            if (isColumnHidden(column))
                continue;
            if (m_filter.keyword.matches(textForEntry(item, column))) {
                matched = true;
                break;
            }
        }
        // Inverted filtering keeps the rows that do *not* match (REQ-FILTER-09).
        if (matched == m_filter.invertKeyword)
            return false;
    }
    return true;
}

void LogTableModel::rebuildFilter()
{
    m_filterActive = m_filter.isActive() && m_provider && m_provider->rowCount() > 0;
    m_visibleRows.clear();
    clearRowCaches();

    if (!m_filterActive || !m_provider)
        return;

    const int rows = m_provider->rowCount();
    m_visibleRows.reserve(static_cast<size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        if (entryMatches(m_provider->entryAt(row)))
            m_visibleRows.push_back(row);
    }
}

void LogTableModel::clearRowCaches()
{
    m_findCache.clear();
    m_tokenCache.clear();
    m_findRows.clear();
    m_findRowsValid = false;
}

void LogTableModel::setFindMatcher(const Matcher &matcher)
{
    m_find = matcher;
    clearRowCaches();
}

quint64 LogTableModel::cacheKey(int row, int column) const
{
    return (static_cast<quint64>(static_cast<quint32>(row)) << 20)
        | static_cast<quint64>(column & 0xFFFF);
}

QVector<MatchRange> LogTableModel::findRanges(int row, int column) const
{
    if (m_find.isEmpty() || !m_find.isValid())
        return {};

    const quint64 key = cacheKey(row, column);
    const auto cached = m_findCache.constFind(key);
    if (cached != m_findCache.constEnd())
        return *cached;

    if (m_findCache.size() > kMaxCachedRanges)
        m_findCache.clear();

    const QVector<MatchRange> ranges = m_find.ranges(cellText(row, column));
    m_findCache.insert(key, ranges);
    return ranges;
}

void LogTableModel::ensureFindRows() const
{
    if (m_findRowsValid)
        return;
    m_findRowsValid = true;
    m_findRows.clear();
    if (m_find.isEmpty() || !m_find.isValid() || !m_provider)
        return;

    const int rows = rowCount();
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < m_columns.size(); ++column) {
            if (isColumnHidden(column))
                continue;
            if (!findRanges(row, column).isEmpty()) {
                m_findRows.push_back(row);
                break;
            }
        }
    }
}

int LogTableModel::findMatchRowCount() const
{
    ensureFindRows();
    return static_cast<int>(m_findRows.size());
}

int LogTableModel::findRowAt(int ordinal) const
{
    ensureFindRows();
    if (ordinal < 0 || ordinal >= static_cast<int>(m_findRows.size()))
        return -1;
    return m_findRows[static_cast<size_t>(ordinal)];
}

int LogTableModel::findOrdinalOf(int viewRow) const
{
    ensureFindRows();
    for (size_t i = 0; i < m_findRows.size(); ++i) {
        if (m_findRows[i] == viewRow)
            return static_cast<int>(i) + 1;
    }
    return -1;
}

const QVector<TokenSpan> &LogTableModel::tokenSpans(int row, int column) const
{
    static const QVector<TokenSpan> empty;
    if (columnKind(column) != ColumnKind::Message)
        return empty;

    const quint64 key = cacheKey(row, column);
    const auto cached = m_tokenCache.constFind(key);
    if (cached != m_tokenCache.constEnd())
        return *cached;

    if (m_tokenCache.size() > kMaxCachedRanges)
        m_tokenCache.clear();

    const auto inserted = m_tokenCache.insert(key, SnippetTokenizer::tokenize(cellText(row, column)));
    return *inserted;
}

} // namespace lv
