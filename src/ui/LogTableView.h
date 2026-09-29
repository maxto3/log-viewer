#pragma once

#include <QHash>
#include <QPair>
#include <QSet>
#include <QTableView>
#include <QVector>

class QToolButton;
class QColor;

namespace lv {

class LogItemDelegate;
class LogHeaderView;
class LogTableModel;
class HighlightTheme;

/// Log table view: row selection feeding the detail pane, Enter/Space row
/// expansion, double click to copy a cell and a small context menu.
///
/// Two row sizing modes are supported:
///  * default - two line rows, long content is truncated with "…";
///  * full content - every row is sized to its complete content so nothing is
///    truncated (used when the details pane is switched off).
class LogTableView : public QTableView
{
    Q_OBJECT

public:
    explicit LogTableView(QWidget *parent = nullptr);

    void setLogModel(LogTableModel *model);
    LogTableModel *logModel() const;

    void setRowHeightLines(int lines);
    void setDarkTheme(bool dark);
    void setTableFont(const QFont &font);
    void setHeaderFont(const QFont &font);
    /// VSCode palette for embedded snippets + Find highlight colours.
    void setHighlightTheme(const HighlightTheme *theme);
    void setKeywordColors(const QColor &background, const QColor &foreground);
    /// Column widths (title -> pixel width) for the current document.
    QVariantMap columnWidths() const;
    void restoreColumnWidths(const QVariantMap &widths);
    /// Column index of the message column (-1 when unknown).
    int messageColumnIndex() const { return messageColumn(); }

    /// Switches between the truncated two line layout and the full content layout.
    void setFullContentMode(bool enabled);
    bool fullContentMode() const { return m_fullContent; }

    void applyColumnWidths();
    /// Mirrors the model's column visibility onto the header sections and moves
    /// the stretch to a visible column (REQ-TABLE-11).
    void applyColumnVisibility();
    /// Suspends the content based height passes while a document is being
    /// attached (model reset, column visibility, widths, details pane). Resuming
    /// runs exactly one height pass instead of one per intermediate step.
    void setHeightPassSuspended(bool suspended);
    /// Sizes the columns from the current content (REQ-TABLE-05 / context menu
    /// "Auto-fit Columns"): the time stamp always keeps its full width, the
    /// flexible column (message, see stretchColumn()) keeps whatever is left and
    /// every other column is scaled down proportionally to its content length when
    /// the table is too narrow for all of them.
    void autoFitColumns();
    /// Column that absorbs the remaining width: the message column when it is
    /// visible, otherwise the last visible column.
    int stretchColumn() const;
    void clearExpansion();
    bool isRowExpanded(int row) const;

    /// Live monitoring helpers: follow the tail and count rows the user has not
    /// seen yet (the "new lines" button).
    bool isAtBottom() const;
    void scrollToBottomNow();
    void setPendingNewRows(int count);
    int pendingNewRows() const { return m_pendingNewRows; }

    /// Diagnostic helper: content height of a row computed from scratch (no cache).
    int freshContentHeight(int row) const;

    void copyCell(const QModelIndex &index);
    void copyCurrentRow();
    void copySelection();

signals:
    void copyFeedback(const QString &message);
    void findNextRequested();
    void findPreviousRequested();
    /// Emitted when the user activates a row (mouse click or keyboard
    /// navigation) - programmatic selection changes do not emit this.
    void rowActivatedByUser(int row);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void resizeEvent(QResizeEvent *event) override;
    /// Width Qt needs to fit \a column to its content; deliberately overridden:
    /// the base implementation samples only a small window of rows.
    int sizeHintForColumn(int column) const override;

private slots:
    void onDoubleClicked(const QModelIndex &index);
    void onSectionResized(int logicalIndex, int oldSize, int newSize);

private:
    void toggleExpansion(int row);
    void positionNewRowsButton();    /// Recomputes (and applies) the row heights after a mode or width change.
    void applyRowHeights();
    /// Sizes the rows that are currently visible.
    void updateVisibleRowHeights();
    /// Defers a height refresh until the header has settled its stretched
    /// column widths (the first layout pass still reports the default width).
    void scheduleHeightRefresh();
    int messageColumnWidth() const;
    /// True when the message column exists and is not hidden: content based row
    /// heights (full content mode, row expansion) only make sense then.
    bool messageColumnVisible() const;
    /// Width the header title of \a column needs (bold font plus the header's side
    /// padding): the lower bound of a content fitted column.
    int headerTitleWidth(int column) const;
    /// Widest content hint of \a column over the document: the delegate's size
    /// hint is sampled from the first rows and then evenly spread over the rest
    /// (Qt's sizeHintForColumn() would only look at a limited window).
    int contentWidthHint(int column) const;
    /// Moves the header's stretch section when the stretch column changed.
    void updateStretchColumn();
    /// Content height of \a row for the given message column width (cached).
    /// \a widths is the column width snapshot of the running pass (nullptr =
    /// take a fresh snapshot): one pass must not mix widths.
    int rowHeightFor(int row, int width, const QVector<int> *widths = nullptr) const;
    /// Content height of \a row from scratch: the tallest visible cell decides,
    /// so a long Target with a short message cannot overflow the row.
    int contentHeightForRow(int row, const QVector<int> &widths) const;
    /// Current width of every column (0 = hidden); changes here invalidate the
    /// cached content heights.
    QVector<int> columnWidthSnapshot() const;
    int estimatedDefaultRowHeight(int rows) const;
    void invalidateHeightCache();
    int messageColumn() const;
    qint64 rowKey(int row) const;

    LogItemDelegate *m_delegate = nullptr;
    LogHeaderView *m_header = nullptr;
    QToolButton *m_newRowsButton = nullptr;
    int m_pendingNewRows = 0;
    QSet<qint64> m_expandedRows;
    /// Cache of "content height" per (row key, message column width).
    mutable QHash<QPair<qint64, int>, int> m_heightCache;
    /// Rows whose height differs from the default section size.
    QHash<int, int> m_appliedHeights;
    int m_rowHeightLines = 2;
    bool m_updatingHeights = false;
    bool m_fullContent = false;
    bool m_heightRefreshPending = false;
    /// Greater than 0 while a bulk document change suspends the height passes.
    int m_heightPassSuspends = 0;
    /// Visible column widths used by the last height pass (empty = none yet).
    /// Any column can change the content height, so the deferred self check
    /// compares the whole snapshot, not only the message column.
    QVector<int> m_heightPassWidths;
    /// Section with QHeaderView::Stretch (message column, or the last visible
    /// column when the message column is hidden).
    int m_stretchColumn = -1;

    /// Documents up to this many rows get exact heights for every row in full
    /// content mode; larger documents estimate and refine them on scrolling.
    static constexpr int kExactHeightRowLimit = 50'000;
    /// Above this row count a column resize only re-sizes the visible rows.
    static constexpr int kLiveResizeRowLimit = 5'000;
};

} // namespace lv
