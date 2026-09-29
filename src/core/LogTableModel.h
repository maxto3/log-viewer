#pragma once

#include "core/ColumnKind.h"
#include "core/FilterSpec.h"
#include "core/IEntryProvider.h"
#include "highlight/HighlightTheme.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QSet>
#include <QVector>

#include <vector>

namespace lv {

/// Table model over an IEntryProvider.
///
/// Besides exposing columns it owns the two row based pipelines:
///  * the Filter (level / time / keyword) which computes the visible row map, so
///    the views see a smaller model (REQ-FILTER);
///  * the Find matcher, which only produces highlight ranges and row navigation
///    (REQ-FIND). Snippet tokens for the VSCode colours are cached here as well.
class LogTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Role {
        /// Untruncated text of the cell (what Ctrl+C / the context menu copy).
        FullTextRole = Qt::UserRole + 1,
        /// ColumnKind of the column (int).
        ColumnKindRole
    };

    explicit LogTableModel(QObject *parent = nullptr);

    void setProvider(EntryProviderPtr provider);
    EntryProviderPtr provider() const { return m_provider; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    ColumnKind columnKind(int column) const;
    QString extraKey(int column) const;
    /// Stable identity of a column ("thread", "extra:<name>"), independent of the
    /// translated title: the Columns menu and the persisted visibility use it.
    QString columnKey(int column) const;
    /// Untruncated text for clipboard operations and the detail pane.
    QString cellText(int row, int column) const;
    const LogEntry *entry(int row) const;
    /// Preferred initial column width in pixels.
    int preferredColumnWidth(int column) const;
    /// Re-emits the horizontal header data so the views repaint the titles with
    /// the current translator (REQ-I18N-02: the titles are translated on demand,
    /// a language switch does not rebuild the columns).
    void retranslateHeaders();

    // Column visibility (REQ-TABLE-11) --------------------------------------
    /// Hidden columns keep their data and width, but the view hides them and the
    /// keyword filter / Find skip them, so "hidden" always means "not searched".
    bool isColumnHidden(int column) const;
    void setColumnHidden(int column, bool hidden);
    QSet<QString> hiddenColumnKeys() const { return m_hiddenColumns; }
    void setHiddenColumnKeys(const QSet<QString> &keys);

    /// Notifies the views that rows were appended to the provider (live
    /// monitoring); existing rows keep their identity.
    void appendRows(int firstRow);

    // Filtering -------------------------------------------------------------
    void setFilterSpec(const FilterSpec &spec);
    const FilterSpec &filterSpec() const { return m_filter; }
    bool isFiltered() const { return m_filterActive; }
    /// View row -> source row inside the provider.
    int sourceRow(int row) const;
    /// Source row -> view row, or -1 when the row is filtered out.
    int visibleRowOf(int sourceRow) const;

    // Find highlighting and navigation --------------------------------------
    void setFindMatcher(const Matcher &matcher);
    Matcher findMatcher() const { return m_find; }
    QVector<MatchRange> findRanges(int row, int column) const;
    /// Number of (visible) rows containing at least one find match.
    int findMatchRowCount() const;
    /// View row of the \a ordinal-th (0 based) matching row, or -1.
    int findRowAt(int ordinal) const;
    /// 1-based ordinal of \a viewRow among the matching rows, or -1.
    int findOrdinalOf(int viewRow) const;

    // Snippet tokens (VSCode colours) ---------------------------------------
    const QVector<TokenSpan> &tokenSpans(int row, int column) const;

signals:
    /// Emitted after the visibility of at least one column changed; the view
    /// mirrors it onto the header sections (REQ-TABLE-11).
    void columnsVisibilityChanged();

private:
    void rebuildColumns();
    void rebuildFilter();
    bool entryMatches(const LogEntry &entry) const;
    /// Renders one column for an entry without going through the row mapping.
    QString textForEntry(const LogEntry &entry, int column) const;
    void clearRowCaches();
    void ensureFindRows() const;
    quint64 cacheKey(int row, int column) const;

    struct Column {
        ColumnKind kind = ColumnKind::Message;
        QString extraKey;
        int preferredWidth = 120;
    };

    QVector<Column> m_columns;
    EntryProviderPtr m_provider;
    /// Keys (see columnKey()) of the columns switched off in the Columns menu.
    QSet<QString> m_hiddenColumns;

    FilterSpec m_filter;
    bool m_filterActive = false;
    std::vector<int> m_visibleRows;      ///< source rows, only when filtering

    Matcher m_find;
    mutable QHash<quint64, QVector<MatchRange>> m_findCache;
    mutable QHash<quint64, QVector<TokenSpan>> m_tokenCache;
    mutable std::vector<int> m_findRows; ///< view rows containing a find match
    mutable bool m_findRowsValid = false;
};

} // namespace lv
