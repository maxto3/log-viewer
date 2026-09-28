#include "ui/LogTableView.h"

#include "core/LogTableModel.h"
#include "ui/LogHeaderView.h"
#include "ui/LogItemDelegate.h"

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>

namespace lv {
namespace {

/// Minimum width a column may be squeezed to when space is tight; the message
/// column keeps at least 40% of the table (see applyColumnWidths()).
int minimumColumnWidth(ColumnKind kind)
{
    switch (kind) {
    case ColumnKind::Line:   return 46;
    case ColumnKind::Time:   return 215;   // a full "yyyy-MM-dd HH:mm:ss.zzz" stamp
    case ColumnKind::Level:  return 68;
    case ColumnKind::Thread: return 112;
    case ColumnKind::Target: return 120;
    case ColumnKind::Pid:    return 56;
    case ColumnKind::Host:   return 90;
    case ColumnKind::File:   return 110;
    case ColumnKind::Extra:  return 80;
    case ColumnKind::Message: break;
    }
    return 120;
}

/// Upper bound for a column fitted to its content (auto-fit and the double click
/// on a section border); one very long log line must not swallow the table.
constexpr int kMaxFittedColumnWidth = 600;
/// Absolute floor for a column when the content proportional scaling in a narrow
/// table would push it below this (the message column keeps its share instead).
constexpr int kMinTightColumnWidth = 24;
/// A fitted column is this many pixels wider than the measured content:
/// QTextLayout rounds slightly differently than QFontMetrics and the last glyph
/// must not wrap (REQ-TABLE-05: no unnecessary blank space).
constexpr int kFitSafetyPixels = 2;
/// Floor for a fitted column, independent of the content (a column that only
/// carries its header title must stay visible).
constexpr int kMinFittedColumnWidth = 36;
/// Rows measured contiguously at the top of the document when a column is fitted
/// (the first entries often carry distinctive values, for example the level).
constexpr int kAutoFitHeadRows = 500;
/// Rows measured evenly spread over the rest of the document. Qt's own
/// sizeHintForColumn() only inspects a limited window near the top and can miss a
/// longer value further down (REQ-TABLE-05).
constexpr int kAutoFitSpreadRows = 3500;

} // namespace

LogTableView::LogTableView(QWidget *parent)
    : QTableView(parent)
{
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    setWordWrap(true);
    setAlternatingRowColors(true);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setCornerButtonEnabled(false);
    setTextElideMode(Qt::ElideRight);
    setAcceptDrops(false);      // drops are handled by the main window

    m_header = new LogHeaderView(Qt::Horizontal, this);
    setHorizontalHeader(m_header);
    verticalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(40);

    m_delegate = new LogItemDelegate(this);
    m_delegate->setExpansionProbe([this](int row) { return isRowExpanded(row); });
    setItemDelegate(m_delegate);

    connect(this, &QAbstractItemView::doubleClicked, this, &LogTableView::onDoubleClicked);
    connect(this, &QAbstractItemView::clicked, this, [this](const QModelIndex &index) {
        if (index.isValid())
            emit rowActivatedByUser(index.row());
    });
    connect(m_header, &QHeaderView::sectionResized, this, &LogTableView::onSectionResized);

    // "New lines" button for live monitoring (spec.md REQ-MON-03): shown while
    // the user has scrolled away from the tail.
    m_newRowsButton = new QToolButton(this);
    m_newRowsButton->setAutoRaise(false);
    m_newRowsButton->setFocusPolicy(Qt::NoFocus);
    m_newRowsButton->hide();
    connect(m_newRowsButton, &QToolButton::clicked, this, [this] {
        scrollToBottomNow();
        setPendingNewRows(0);
    });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        if (m_pendingNewRows > 0 && isAtBottom())
            setPendingNewRows(0);
    });
}

bool LogTableView::isAtBottom() const
{
    const QScrollBar *bar = verticalScrollBar();
    return bar->value() >= bar->maximum() - 1;
}

void LogTableView::scrollToBottomNow()
{
    scrollToBottom();
}

void LogTableView::setPendingNewRows(int count)
{
    m_pendingNewRows = qMax(0, count);
    if (m_pendingNewRows == 0) {
        m_newRowsButton->hide();
        return;
    }
    m_newRowsButton->setText(tr("New lines: %1 ▼").arg(m_pendingNewRows));
    m_newRowsButton->adjustSize();
    positionNewRowsButton();
    m_newRowsButton->show();
    m_newRowsButton->raise();
}

void LogTableView::positionNewRowsButton()
{
    if (!m_newRowsButton)
        return;
    const QSize hint = m_newRowsButton->sizeHint();
    const int x = viewport()->width() - hint.width() - 28;
    const int y = viewport()->height() - hint.height() - 28;
    m_newRowsButton->move(qMax(8, x), qMax(8, y));
}

void LogTableView::setLogModel(LogTableModel *model)
{
    setModel(model);
    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, [this] {
            clearExpansion();
            invalidateHeightCache();
            applyColumnWidths();
            applyRowHeights();
        });
        connect(model, &LogTableModel::columnsVisibilityChanged,
                this, &LogTableView::applyColumnVisibility);
    }
    applyColumnVisibility();
    applyColumnWidths();
    applyRowHeights();
}

LogTableModel *LogTableView::logModel() const
{
    return qobject_cast<LogTableModel *>(model());
}

void LogTableView::setRowHeightLines(int lines)
{
    m_rowHeightLines = qMax(1, lines);
    m_delegate->setRowHeightLines(m_rowHeightLines);
    invalidateHeightCache();
    applyRowHeights();
}

void LogTableView::setFullContentMode(bool enabled)
{
    if (m_fullContent == enabled)
        return;
    m_fullContent = enabled;
    m_delegate->setFullContentMode(enabled);
    invalidateHeightCache();
    applyRowHeights();
    viewport()->update();
}

void LogTableView::setDarkTheme(bool dark)
{
    m_delegate->setDarkTheme(dark);
    m_header->setDarkTheme(dark);
    viewport()->update();
}

void LogTableView::setTableFont(const QFont &font)
{
    setFont(font);
    m_delegate->setCellFont(font);
    invalidateHeightCache();
    applyRowHeights();
}

void LogTableView::setHeaderFont(const QFont &font)
{
    m_header->setHeaderFont(font);
}

void LogTableView::setHighlightTheme(const HighlightTheme *theme)
{
    m_delegate->setHighlightTheme(theme);
}

void LogTableView::setKeywordColors(const QColor &background, const QColor &foreground)
{
    m_delegate->setKeywordColors(background, foreground);
}

QVariantMap LogTableView::columnWidths() const
{
    QVariantMap widths;
    LogTableModel *model = logModel();
    if (!model)
        return widths;
    const int message = messageColumn();
    for (int column = 0; column < model->columnCount(); ++column) {
        if (column == message)
            continue;
        const QString title = model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        if (!title.isEmpty())
            widths.insert(title, columnWidth(column));
    }
    return widths;
}

void LogTableView::restoreColumnWidths(const QVariantMap &widths)
{
    LogTableModel *model = logModel();
    if (!model || widths.isEmpty())
        return;
    const int message = messageColumn();
    for (int column = 0; column < model->columnCount(); ++column) {
        if (column == message)
            continue;
        const QString title = model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        const auto it = widths.constFind(title);
        if (it == widths.constEnd())
            continue;
        const int width = it->toInt();
        if (width > 0)
            setColumnWidth(column, width);
    }
    invalidateHeightCache();
    updateVisibleRowHeights();
}

void LogTableView::applyColumnWidths()
{
    LogTableModel *model = logModel();
    if (!model)
        return;
    m_header->setSectionResizeMode(QHeaderView::Interactive);

    // The stretch column takes the remaining width: the message column normally,
    // otherwise the last visible column (REQ-TABLE-11).
    const int stretch = stretchColumn();
    const int columns = model->columnCount();

    // The message column keeps at least 40% of the table so that truncation and
    // the "…" indicator stay meaningful, even when the details pane is open.
    const int total = qMax(600, viewport()->width());
    int fixedWidth = 0;
    for (int column = 0; column < columns; ++column) {
        if (column != stretch && !model->isColumnHidden(column))
            fixedWidth += model->preferredColumnWidth(column);
    }
    const int minMessageWidth = qMax(240, total * 2 / 5);
    qreal scale = 1.0;
    if (fixedWidth + minMessageWidth > total && fixedWidth > 0)
        scale = qMax(0.45, static_cast<qreal>(total - minMessageWidth) / fixedWidth);

    for (int column = 0; column < columns; ++column) {
        if (column == stretch || model->isColumnHidden(column))
            continue;
        const int scaled = static_cast<int>(model->preferredColumnWidth(column) * scale);
        const int width = qMax(minimumColumnWidth(model->columnKind(column)), scaled);
        setColumnWidth(column, width);
    }

    if (m_stretchColumn != stretch && m_stretchColumn >= 0 && m_stretchColumn < columns)
        m_header->setSectionResizeMode(m_stretchColumn, QHeaderView::Interactive);
    m_stretchColumn = stretch;
    if (stretch >= 0)
        m_header->setSectionResizeMode(stretch, QHeaderView::Stretch);

    invalidateHeightCache();
}

int LogTableView::stretchColumn() const
{
    LogTableModel *model = logModel();
    if (!model)
        return -1;
    const int message = messageColumn();
    if (message >= 0 && !model->isColumnHidden(message))
        return message;
    for (int column = model->columnCount() - 1; column >= 0; --column) {
        if (!model->isColumnHidden(column))
            return column;
    }
    return -1;
}

void LogTableView::updateStretchColumn()
{
    LogTableModel *model = logModel();
    if (!model)
        return;
    const int wanted = stretchColumn();
    if (wanted == m_stretchColumn)
        return;
    if (m_stretchColumn >= 0 && m_stretchColumn < model->columnCount())
        m_header->setSectionResizeMode(m_stretchColumn, QHeaderView::Interactive);
    m_stretchColumn = wanted;
    if (wanted >= 0)
        m_header->setSectionResizeMode(wanted, QHeaderView::Stretch);
}

void LogTableView::applyColumnVisibility()
{
    LogTableModel *model = logModel();
    if (!model)
        return;
    for (int column = 0; column < model->columnCount(); ++column) {
        const bool hidden = model->isColumnHidden(column);
        if (isColumnHidden(column) != hidden)
            setColumnHidden(column, hidden);
    }
    updateStretchColumn();
    invalidateHeightCache();
    applyRowHeights();
}

int LogTableView::messageColumn() const
{
    LogTableModel *model = logModel();
    if (!model)
        return -1;
    for (int column = 0; column < model->columnCount(); ++column) {
        if (model->columnKind(column) == ColumnKind::Message)
            return column;
    }
    return -1;
}

qint64 LogTableView::rowKey(int row) const
{
    // The key must be unique per entry and stable across filtering: the provider
    // row index is used, combined with the source index for merged documents.
    // (Using the first physical line would collide for document parsed formats
    // such as the event log XML export, where many entries share one line.)
    LogTableModel *model = logModel();
    if (!model)
        return row;

    const int source = model->sourceRow(row);
    const LogEntry *entry = model->entry(row);
    const int sourceIndex = entry ? entry->sourceIndex : 0;
    const qint64 providerRow = source >= 0 ? source : row;
    return (static_cast<qint64>(sourceIndex) << 40) | providerRow;
}

bool LogTableView::isRowExpanded(int row) const
{
    return m_expandedRows.contains(rowKey(row));
}

void LogTableView::clearExpansion()
{
    if (m_expandedRows.isEmpty())
        return;
    m_expandedRows.clear();
    updateVisibleRowHeights();
}

void LogTableView::toggleExpansion(int row)
{
    if (row < 0)
        return;
    const qint64 key = rowKey(row);
    if (m_expandedRows.contains(key))
        m_expandedRows.remove(key);
    else
        m_expandedRows.insert(key);
    updateVisibleRowHeights();
    viewport()->update();
}

void LogTableView::invalidateHeightCache()
{
    m_heightCache.clear();
}

int LogTableView::messageColumnWidth() const
{
    const int message = messageColumn();
    return message >= 0 ? columnWidth(message) : viewport()->width();
}

bool LogTableView::messageColumnVisible() const
{
    LogTableModel *model = logModel();
    const int message = messageColumn();
    return model && message >= 0 && !model->isColumnHidden(message);
}

void LogTableView::scheduleHeightRefresh()
{
    if (m_heightRefreshPending)
        return;
    m_heightRefreshPending = true;
    QTimer::singleShot(0, this, [this] {
        m_heightRefreshPending = false;
        if (m_updatingHeights) {
            // A pass is still running: try again on the next event loop turn.
            scheduleHeightRefresh();
            return;
        }
        if (!m_fullContent && m_expandedRows.isEmpty())
            return;
        if (!messageColumnVisible())
            return;               // heights do not depend on content any more
        if (messageColumnWidth() == m_heightPassWidth)
            return;
        invalidateHeightCache();
        applyRowHeights();
    });
}

int LogTableView::rowHeightFor(int row, int width) const
{
    LogTableModel *model = logModel();
    if (!model || row < 0 || row >= model->rowCount())
        return m_delegate->defaultRowHeight();

    const QPair<qint64, int> key{rowKey(row), width};

    const auto cached = m_heightCache.constFind(key);
    if (cached != m_heightCache.constEnd())
        return *cached;

    const int message = messageColumn();
    const QModelIndex index = model->index(row, qMax(0, message));
    const int height = m_delegate->contentRowHeight(index, width);
    m_heightCache.insert(key, height);
    return height;
}

int LogTableView::freshContentHeight(int row) const
{
    LogTableModel *model = logModel();
    if (!model || row < 0 || row >= model->rowCount())
        return 0;
    const int message = messageColumn();
    const int width = message >= 0 ? columnWidth(message) : viewport()->width();
    return m_delegate->contentRowHeight(model->index(row, qMax(0, message)), width);
}

int LogTableView::estimatedDefaultRowHeight(int rows) const
{
    const int defaultHeight = m_delegate->defaultRowHeight();
    if (rows <= 0)
        return defaultHeight;

    const int width = messageColumnWidth();
    const int samples = qMin(rows, 1000);
    qint64 total = 0;
    for (int i = 0; i < samples; ++i) {
        const int row = (rows > samples) ? static_cast<int>(qint64(i) * rows / samples) : i;
        total += rowHeightFor(row, width);
    }
    const int average = static_cast<int>(total / samples);
    // Keep the estimate within a sensible range so the scroll bar stays usable.
    return qBound(defaultHeight, average, defaultHeight * 20);
}

void LogTableView::applyRowHeights()
{
    if (m_updatingHeights)
        return;
    m_updatingHeights = true;

    LogTableModel *model = logModel();
    const int rows = model ? model->rowCount() : 0;
    const int defaultHeight = m_delegate->defaultRowHeight();
    verticalHeader()->setDefaultSectionSize(defaultHeight);

    // Restore the rows whose height we changed earlier.
    for (auto it = m_appliedHeights.constBegin(); it != m_appliedHeights.constEnd(); ++it) {
        const int row = it.key();
        if (row >= 0 && row < rows && rowHeight(row) != defaultHeight)
            setRowHeight(row, defaultHeight);
    }
    m_appliedHeights.clear();

    // One width snapshot for the whole pass: mixing widths would leave rows with
    // inconsistent heights (regression: rows truncated after the first open).
    const int passWidth = messageColumnWidth();

    if (m_fullContent && rows > 0 && messageColumnVisible()) {
        if (rows <= kExactHeightRowLimit) {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            viewport()->setUpdatesEnabled(false);
            for (int row = 0; row < rows; ++row) {
                const int height = rowHeightFor(row, passWidth);
                setRowHeight(row, height);
                if (height != defaultHeight)
                    m_appliedHeights.insert(row, height);
            }
            viewport()->setUpdatesEnabled(true);
            viewport()->update();
            QApplication::restoreOverrideCursor();
        } else {
            // Too many rows for exact heights: estimate the default height from
            // a sample; rows refine themselves as they scroll into view.
            verticalHeader()->setDefaultSectionSize(estimatedDefaultRowHeight(rows));
        }
    }

    m_updatingHeights = false;
    m_heightPassWidth = passWidth;
    updateVisibleRowHeights();
    scheduleHeightRefresh();
}

void LogTableView::updateVisibleRowHeights()
{
    if (m_updatingHeights)
        return;
    m_updatingHeights = true;

    LogTableModel *model = logModel();
    const int defaultHeight = m_delegate->defaultRowHeight();
    const int message = messageColumn();
    const int messageWidth = message >= 0 ? columnWidth(message) : viewport()->width();
    const bool messageVisible = messageColumnVisible();

    int first = rowAt(0);
    if (first < 0)
        first = 0;
    int last = rowAt(viewport()->height() - 1);
    if (last < 0)
        last = model ? model->rowCount() - 1 : -1;

    for (int row = first; row <= last && row >= 0 && model && row < model->rowCount(); ++row) {
        int height = defaultHeight;
        if (messageVisible && m_fullContent) {
            height = rowHeightFor(row, messageWidth);
        } else if (messageVisible && isRowExpanded(row)) {
            height = m_delegate->contentRowHeight(model->index(row, qMax(0, message)), messageWidth);
        }
        if (rowHeight(row) != height) {
            setRowHeight(row, height);
            if (height != defaultHeight)
                m_appliedHeights.insert(row, height);
            else
                m_appliedHeights.remove(row);
        }
    }
    m_updatingHeights = false;
}

void LogTableView::onSectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(oldSize);
    Q_UNUSED(newSize);
    if (logicalIndex != messageColumn())
        return;

    invalidateHeightCache();

    // A resize while a height pass is running must not be dropped: remember it and
    // let the pass (or the deferred check) redo the work with the final width.
    if (m_updatingHeights) {
        m_heightRefreshNeeded = true;
        scheduleHeightRefresh();
        return;
    }

    LogTableModel *model = logModel();
    const int rows = model ? model->rowCount() : 0;
    if (m_fullContent && rows > 0 && rows <= kLiveResizeRowLimit)
        applyRowHeights();
    else
        updateVisibleRowHeights();
}

void LogTableView::scrollContentsBy(int dx, int dy)
{
    QTableView::scrollContentsBy(dx, dy);
    updateVisibleRowHeights();
}

void LogTableView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    updateVisibleRowHeights();
    positionNewRowsButton();
}

void LogTableView::keyPressEvent(QKeyEvent *event)
{
    const int rowBefore = currentIndex().isValid() ? currentIndex().row() : -1;

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (currentIndex().isValid() && !m_fullContent) {
            toggleExpansion(currentIndex().row());
            event->accept();
            return;
        }
        break;
    case Qt::Key_F3:
        if (event->modifiers() & Qt::ShiftModifier)
            emit findPreviousRequested();
        else
            emit findNextRequested();
        event->accept();
        return;
    case Qt::Key_C:
        if (event->matches(QKeySequence::Copy)) {
            copySelection();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    QTableView::keyPressEvent(event);

    // Keyboard navigation counts as user activation (spec.md REQ-UI-11).
    const int key = event->key();
    const bool navigationKey = key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_PageUp
        || key == Qt::Key_PageDown || key == Qt::Key_Home || key == Qt::Key_End;
    if (navigationKey && currentIndex().isValid() && currentIndex().row() != rowBefore)
        emit rowActivatedByUser(currentIndex().row());
}

void LogTableView::onDoubleClicked(const QModelIndex &index)
{
    copyCell(index);
}

void LogTableView::copyCell(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    LogTableModel *model = logModel();
    const QString text = model ? model->cellText(index.row(), index.column())
                               : index.data().toString();
    QApplication::clipboard()->setText(text);
    const QString column = model
        ? model->headerData(index.column(), Qt::Horizontal, Qt::DisplayRole).toString()
        : QString();
    emit copyFeedback(tr("Copied %1 characters from \"%2\"").arg(text.size()).arg(column));
}

void LogTableView::copyCurrentRow()
{
    const QModelIndex current = currentIndex();
    if (!current.isValid())
        return;
    LogTableModel *model = logModel();
    if (!model)
        return;

    QStringList cells;
    for (int column = 0; column < model->columnCount(); ++column)
        cells.append(model->cellText(current.row(), column).replace(QLatin1Char('\n'), QLatin1Char(' ')));
    const QString text = cells.join(QLatin1Char('\t'));
    QApplication::clipboard()->setText(text);
    emit copyFeedback(tr("Copied row %1 (%2 characters)").arg(current.row() + 1).arg(text.size()));
}

void LogTableView::copySelection()
{
    const QModelIndexList indexes = selectionModel() ? selectionModel()->selectedIndexes()
                                                     : QModelIndexList();
    if (indexes.isEmpty()) {
        copyCurrentRow();
        return;
    }
    if (indexes.size() == 1) {
        copyCell(indexes.first());
        return;
    }

    LogTableModel *model = logModel();
    if (!model)
        return;

    QMap<int, QMap<int, QString>> rows;
    for (const QModelIndex &index : indexes)
        rows[index.row()].insert(index.column(), model->cellText(index.row(), index.column()));

    QStringList lines;
    for (auto rowIt = rows.constBegin(); rowIt != rows.constEnd(); ++rowIt) {
        QStringList cells;
        for (auto cellIt = rowIt.value().constBegin(); cellIt != rowIt.value().constEnd(); ++cellIt)
            cells.append(QString(cellIt.value()).replace(QLatin1Char('\n'), QLatin1Char(' ')));
        lines.append(cells.join(QLatin1Char('\t')));
    }
    const QString text = lines.join(QLatin1Char('\n'));
    QApplication::clipboard()->setText(text);
    emit copyFeedback(tr("Copied %1 cells (%2 characters)").arg(indexes.size()).arg(text.size()));
}

void LogTableView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid())
        setCurrentIndex(index);

    QMenu menu(this);
    QAction *copyCellAction = menu.addAction(tr("Copy Cell"));
    QAction *copyRowAction = menu.addAction(tr("Copy Row"));
    QAction *copyMessageAction = menu.addAction(tr("Copy Message"));
    menu.addSeparator();
    QAction *autoFitAction = menu.addAction(tr("Auto-fit Columns"));

    copyCellAction->setEnabled(index.isValid());
    copyRowAction->setEnabled(index.isValid());
    copyMessageAction->setEnabled(index.isValid());

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen)
        return;
    if (chosen == copyCellAction) {
        copyCell(index);
    } else if (chosen == copyRowAction) {
        copyCurrentRow();
    } else if (chosen == copyMessageAction) {
        LogTableModel *model = logModel();
        const int message = messageColumn();
        if (model && message >= 0 && index.isValid()) {
            const QString text = model->cellText(index.row(), message);
            QApplication::clipboard()->setText(text);
            emit copyFeedback(tr("Copied message (%1 characters)").arg(text.size()));
        }
    } else if (chosen == autoFitAction) {
        autoFitColumns();
    }
}

int LogTableView::contentWidthHint(int column) const
{
    LogTableModel *model = logModel();
    QAbstractItemDelegate *delegate = itemDelegate();
    if (!model || !delegate || column < 0 || column >= model->columnCount())
        return 0;

    const int rows = model->rowCount();
    if (rows <= 0)
        return 0;

    // Measure the delegate's size hint for a row; the delegate knows the cell
    // padding and the extra width the level chips need.
    QStyleOptionViewItem option;
    option.rect = QRect();
    const auto hintAt = [&](int row) {
        return delegate->sizeHint(option, model->index(row, column)).width();
    };

    int widest = 0;
    const int head = qMin(rows, kAutoFitHeadRows);
    for (int row = 0; row < head; ++row)
        widest = qMax(widest, hintAt(row));
    if (rows > head) {
        const int spread = rows - head;
        const int samples = qMin(spread, kAutoFitSpreadRows);
        for (int i = 0; i < samples; ++i) {
            const int row = head + static_cast<int>(qint64(i) * spread / samples);
            widest = qMax(widest, hintAt(row));
        }
    }
    return widest;
}

int LogTableView::sizeHintForColumn(int column) const
{
    // Overrides Qt's implementation, which only looks at a limited window of rows:
    // a column fitted to its content has to cover the whole document
    // (REQ-TABLE-05).
    return qBound(kMinFittedColumnWidth,
                  contentWidthHint(column) + kFitSafetyPixels,
                  kMaxFittedColumnWidth);
}

int LogTableView::headerTitleWidth(int column) const
{
    LogTableModel *model = logModel();
    if (!model)
        return 0;
    const QString title = model->headerData(column, Qt::Horizontal,
                                            Qt::DisplayRole).toString();
    if (title.isEmpty())
        return 0;
    // LogHeaderView draws the (bold) title with 8 px padding on both sides.
    const QFontMetrics metrics(m_header->font());
    return metrics.horizontalAdvance(title) + 16;
}

void LogTableView::autoFitColumns()
{
    LogTableModel *model = logModel();
    if (!model)
        return;

    // The flexible column absorbs whatever is left over; it is sized by the
    // header's Stretch mode, not here (REQ-TABLE-05).
    const int flexible = stretchColumn();
    const int columns = model->columnCount();
    const int total = qMax(400, viewport()->width());

    QVector<int> content(columns, 0);
    QVector<bool> reservedColumn(columns, false);
    int othersSum = 0;
    for (int column = 0; column < columns; ++column) {
        if (column == flexible || model->isColumnHidden(column))
            continue;
        // Truly adaptive: the width the cells need (plus a 2 px rounding guard),
        // never more; a column whose title is wider than its content keeps the
        // title readable.
        const int needed = qMax(sizeHintForColumn(column) + kFitSafetyPixels,
                                headerTitleWidth(column));
        content[column] = qBound(kMinFittedColumnWidth, needed, kMaxFittedColumnWidth);

        // The time stamp and the level chip are atomic: they must never be
        // wrapped or clipped, so those columns are placed before the rest and are
        // not scaled down (REQ-TABLE-05).
        const ColumnKind kind = model->columnKind(column);
        if (kind == ColumnKind::Time || kind == ColumnKind::Level) {
            reservedColumn[column] = true;
            continue;
        }
        othersSum += content.at(column);
    }

    int reserved = 0;
    for (int column = 0; column < columns; ++column) {
        if (!reservedColumn.at(column))
            continue;
        reserved += content.at(column);
        setColumnWidth(column, content.at(column));
    }

    // The flexible (message) column keeps at least the same share as the initial
    // layout so the log text stays readable.
    const int minFlexible = qMax(240, total * 2 / 5);
    const int budget = qMax(0, total - reserved - minFlexible);

    if (othersSum <= budget) {
        // Enough room: every other column shows its content unchanged.
        for (int column = 0; column < columns; ++column) {
            if (content.at(column) > 0 && !reservedColumn.at(column))
                setColumnWidth(column, content.at(column));
        }
    } else if (othersSum > 0) {
        // Too narrow for everything: scale the remaining columns down
        // proportionally to their content length. The reserved columns (time
        // stamp, level chips) and the flexible (message) column keep their widths,
        // so a scaled column may fall below the usual minimum width; the last
        // column absorbs the rounding.
        int last = -1;
        for (int column = columns - 1; column >= 0; --column) {
            if (content.at(column) > 0 && !reservedColumn.at(column)) {
                last = column;
                break;
            }
        }
        int assigned = 0;
        for (int column = 0; column < columns; ++column) {
            if (content.at(column) <= 0 || reservedColumn.at(column))
                continue;
            const int share = static_cast<int>(qint64(budget) * content.at(column) / othersSum);
            const int width = column == last
                ? qMax(kMinTightColumnWidth, budget - assigned)
                : qMax(kMinTightColumnWidth, share);
            setColumnWidth(column, width);
            assigned += width;
        }
    }

    invalidateHeightCache();
    updateVisibleRowHeights();
}

} // namespace lv
