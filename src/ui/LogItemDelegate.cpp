#include "ui/LogItemDelegate.h"

#include "core/LogTableModel.h"
#include "highlight/LevelPalette.h"

#include <QApplication>
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>

namespace lv {
namespace {

constexpr int kHPadding = 7;
constexpr int kVPadding = 3;
/// Upper bound for the width hint of one cell (a single long log line must not
/// produce an absurd section width when a column is fitted to its content).
constexpr int kMaxSizeHintWidth = 2000;
/// A longer line than this already exceeds kMaxSizeHintWidth, so only a prefix is
/// measured (keeps sizeHintForColumn() cheap on huge messages).
constexpr int kMaxMeasuredChars = 512;

/// Number of lines the text needs when wrapped into \a width (capped at \a maxLines).
int visibleLineCount(const QString &text, int width, const QFont &font, int maxLines)
{
    if (text.isEmpty() || maxLines <= 0)
        return 0;

    const QStringList paragraphs = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    int drawn = 0;
    for (const QString &paragraph : paragraphs) {
        QTextLayout layout(paragraph, font);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(width);
            if (++drawn >= maxLines) {
                layout.endLayout();
                return maxLines;
            }
        }
        layout.endLayout();
    }
    return drawn;
}

} // namespace

LogItemDelegate::LogItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    m_theme = &HighlightTheme::forDarkMode(false);
}

void LogItemDelegate::setDarkTheme(bool dark)
{
    m_dark = dark;
}

void LogItemDelegate::setCellFont(const QFont &font)
{
    m_cellFont = font;
}

void LogItemDelegate::setRowHeightLines(int lines)
{
    m_rowHeightLines = qMax(1, lines);
}

void LogItemDelegate::setFullContentMode(bool enabled)
{
    m_fullContent = enabled;
}

void LogItemDelegate::setHighlightTheme(const HighlightTheme *theme)
{
    m_theme = theme ? theme : &HighlightTheme::forDarkMode(m_dark);
}

void LogItemDelegate::setKeywordColors(const QColor &background, const QColor &foreground)
{
    m_keywordBackground = background;
    m_keywordForeground = foreground;
}

void LogItemDelegate::setExpansionProbe(std::function<bool(int)> probe)
{
    m_isExpanded = std::move(probe);
}

QFont LogItemDelegate::cellFont() const
{
    return m_cellFont.family().isEmpty() ? QApplication::font() : m_cellFont;
}

int LogItemDelegate::lineCountForRow(int row) const
{
    if (m_fullContent)
        return kFullContentLineLimit;
    if (m_isExpanded && m_isExpanded(row))
        return kExpandedLineLimit;
    return m_rowHeightLines;
}

int LogItemDelegate::defaultRowHeight() const
{
    return rowHeightForLines(m_rowHeightLines);
}

int LogItemDelegate::lineCountForCell(const QModelIndex &index, int width, int maxLines) const
{
    const QString text = index.data(LogTableModel::FullTextRole).toString();
    if (text.isEmpty())
        return 1;

    const int modeLimit = lineCountForRow(index.row());
    const int limit = maxLines > 0 ? qMin(maxLines, modeLimit) : modeLimit;

    const QFont f = cellFont();
    const int usableWidth = qMax(40, width - kHPadding * 2);

    // Fast path: a single paragraph whose text fits the cell needs no layout.
    // Most cells (Line, Time, Level, Thread, …) take this branch, so sizing a
    // row for every visible column stays cheap.
    if (!text.contains(QLatin1Char('\n'))) {
        const QFontMetrics metrics(f);
        if (metrics.horizontalAdvance(text) <= usableWidth)
            return 1;
    }

    return qMax(1, visibleLineCount(text, usableWidth, f, limit));
}

int LogItemDelegate::rowHeightForLines(int lines) const
{
    const QFontMetrics metrics(cellFont());
    return metrics.lineSpacing() * qMax(1, lines) + kVPadding * 2 + 2;
}

int LogItemDelegate::contentRowHeight(const QModelIndex &index, int width) const
{
    return rowHeightForLines(lineCountForCell(index, width));
}

void LogItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    painter->save();
    // Intersect (never replace) the paint region: the view may have set a clip
    // for the dirty area, and a cell must never draw past its own rectangle.
    painter->setClipRect(option.rect, Qt::IntersectClip);

    const bool selected = option.state & QStyle::State_Selected;
    // Zebra striping (REQ-TABLE-09): alternate rows use the palette's alternate
    // base colour. The delegate fills every cell itself, so the view's
    // alternatingRowColors cannot shine through: the parity of the *displayed*
    // row (after filtering) decides here.
    const bool alternate = (index.row() % 2) == 1;
    const QColor background = selected ? option.palette.highlight().color()
                                       : (alternate ? option.palette.alternateBase().color()
                                                    : option.palette.base().color());
    painter->fillRect(option.rect, background);

    // Cell separators (REQ-TABLE-10): a vertical line at the right edge and a
    // horizontal line at the bottom edge of every cell, so columns and rows stay
    // visually separated. Keep both colours in sync with LogHeaderView so the
    // header columns continue into the table without a visible break.
    const QColor separator = m_dark ? QColor(0x3A, 0x3A, 0x3E) : QColor(0xD9, 0xD9, 0xDD);
    painter->setPen(separator);
    painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

    const auto kind = static_cast<ColumnKind>(index.data(LogTableModel::ColumnKindRole).toInt());
    const QString text = index.data(Qt::DisplayRole).toString();
    const QRect content = option.rect.adjusted(kHPadding, kVPadding, -kHPadding, -kVPadding);
    const QFont font = cellFont();

    if (kind == ColumnKind::Level && !text.isEmpty()) {
        const auto *model = qobject_cast<const LogTableModel *>(index.model());
        const LogEntry *entry = model ? model->entry(index.row()) : nullptr;
        const LevelColors colors = levelColors(entry ? entry->level : LogLevel::Other, m_dark);

        QFont chipFont = font;
        chipFont.setBold(true);
        painter->setFont(chipFont);
        const QFontMetrics metrics(chipFont);
        const int width = qMin(metrics.horizontalAdvance(text) + 16, content.width());
        const int height = qMin(metrics.height() + 2, content.height());
        // Centre the chip inside the first three lines: for very tall rows (full
        // content mode) the chip stays at the top next to the message text.
        const int chipArea = qMin(content.height(), metrics.lineSpacing() * 3);
        const QRect chip(content.left(), content.top() + (chipArea - height) / 2, width, height);

        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.background);
        painter->drawRoundedRect(chip, 3, 3);
        painter->setPen(colors.text);
        painter->drawText(chip, Qt::AlignCenter, text);
        painter->restore();
        return;
    }

    QColor textColor = selected ? option.palette.highlightedText().color()
                                : option.palette.text().color();
    if (kind == ColumnKind::Time)
        textColor = timestampColor(m_dark);

    CellFormats formats;
    formats.keywordBackground = m_keywordBackground;
    formats.keywordForeground = m_keywordForeground;
    if (const auto *model = qobject_cast<const LogTableModel *>(index.model())) {
        formats.keywords = model->findRanges(index.row(), index.column());
        if (kind == ColumnKind::Message) {
            formats.tokens = &model->tokenSpans(index.row(), index.column());
            formats.theme = m_theme;
        }
    }

    painter->setFont(font);
    painter->setPen(textColor);
    drawClampedText(painter, content, text, lineCountForRow(index.row()), background, formats);

    painter->restore();
}

QSize LogItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Width hint for column sizing (REQ-TABLE-05): the widest line of the full
    // text. Returning option.rect.width() here - as this used to - only echoed the
    // width Qt passes in while measuring, so resizeColumnToContents() (context
    // menu "Auto-fit Columns" and double clicking a section border) collapsed
    // every column to roughly the same minimum width.
    Q_UNUSED(option);

    QFont font = cellFont();
    int extra = kHPadding * 2;
    if (static_cast<ColumnKind>(index.data(LogTableModel::ColumnKindRole).toInt())
        == ColumnKind::Level) {
        font.setBold(true);     // level chips are drawn bold (see paint())
        extra += 16;            // horizontal chip padding used by paint()
    }

    const QFontMetrics metrics(font);
    const QString text = index.data(LogTableModel::FullTextRole).toString();
    int content = 0;
    if (!text.isEmpty()) {
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (const QString &line : lines) {
            // Measuring a prefix is enough: anything longer already exceeds the
            // maximum section width below.
            content = qMax(content, metrics.horizontalAdvance(line.left(kMaxMeasuredChars)));
            if (content >= kMaxSizeHintWidth)
                break;
        }
        content = qMin(content, kMaxSizeHintWidth);
    }
    return QSize(content + extra, defaultRowHeight());
}

QVector<QTextLayout::FormatRange> LogItemDelegate::buildFormats(const CellFormats &formats,
                                                               const QString &paragraph,
                                                               int paragraphStart)
{
    QVector<QTextLayout::FormatRange> result;
    const int paragraphEnd = paragraphStart + paragraph.size();

    if (formats.tokens && formats.theme) {
        for (const TokenSpan &span : *formats.tokens) {
            const int start = qMax(span.start, paragraphStart);
            const int end = qMin(span.start + span.length, paragraphEnd);
            if (end <= start)
                continue;
            QTextCharFormat format;
            format.setForeground(formats.theme->color(span.kind));
            result.append({start - paragraphStart, end - start, format});
        }
    }

    // Keyword highlights are appended last so they override snippet colours
    // (spec.md REQ-HL-07).
    for (const MatchRange &range : formats.keywords) {
        const int start = qMax(range.start, paragraphStart);
        const int end = qMin(range.start + range.length, paragraphEnd);
        if (end <= start)
            continue;
        QTextCharFormat format;
        format.setBackground(formats.keywordBackground);
        format.setForeground(formats.keywordForeground);
        result.append({start - paragraphStart, end - start, format});
    }
    return result;
}

QVector<QTextLayout::FormatRange> LogItemDelegate::shiftFormatsToElidedLine(
    const QVector<QTextLayout::FormatRange> &paragraphFormats, int lineStartInParagraph,
    int elidedLength)
{
    QVector<QTextLayout::FormatRange> shifted;
    for (QTextLayout::FormatRange range : paragraphFormats) {
        const int start = qMax(0, range.start - lineStartInParagraph);
        const int end = qMin(range.start + range.length - lineStartInParagraph, elidedLength);
        if (end <= start)
            continue;
        range.start = start;
        range.length = end - start;
        shifted.append(range);
    }
    return shifted;
}

void LogItemDelegate::drawClampedText(QPainter *painter, const QRect &rect, const QString &text,
                                      int maxLines, const QColor &background,
                                      const CellFormats &formats)
{
    if (text.isEmpty() || maxLines <= 0 || rect.width() <= 0 || rect.height() <= 0)
        return;

    const QFont font = painter->font();
    const QFontMetrics metrics(font);
    const QStringList paragraphs = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    qreal y = rect.top();
    int drawn = 0;
    int paragraphStart = 0;
    for (int p = 0; p < paragraphs.size() && drawn < maxLines; ++p) {
        const QString &paragraph = paragraphs.at(p);

        QTextLayout layout(paragraph, font);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();

        QVector<QTextLine> lines;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(rect.width());
            // Line positions are relative to the layout origin, which is the
            // paragraph's top: use the paragraph local index, not the global one
            // (the global offset is already applied through "y" below).
            line.setPosition(QPointF(0, lines.size() * metrics.lineSpacing()));
            lines.append(line);
            if (drawn + lines.size() >= maxLines)
                break;
        }
        layout.endLayout();

        if (lines.isEmpty()) {
            paragraphStart += paragraph.size() + 1;
            continue;
        }

        const bool paragraphCut = lines.last().textStart() + lines.last().textLength() < paragraph.size();
        const bool moreParagraphs = p < paragraphs.size() - 1;
        const bool reachesLimit = (drawn + lines.size()) >= maxLines;

        const QVector<QTextLayout::FormatRange> ranges =
            buildFormats(formats, paragraph, paragraphStart);

        painter->save();
        // IntersectClip: the paragraph clip may be taller than the cell when the
        // row is shorter than this column needs. ReplaceClip (the default) would
        // override the cell rectangle set by paint() and let the text spill over
        // the row separator.
        painter->setClipRect(QRectF(rect.left(), y, rect.width(),
                                    metrics.lineSpacing() * lines.size() + 1),
                             Qt::IntersectClip);
        layout.draw(painter, QPointF(rect.left(), y), ranges);
        painter->restore();

        if (reachesLimit && (paragraphCut || moreParagraphs)) {
            // Redraw the last visible line with a trailing ellipsis. The text is
            // elided starting at that line, so the remaining content of the
            // paragraph (not just the already fitted line) is what gets elided.
            const QTextLine &last = lines.last();
            const QRectF lastRect(rect.left(), y + last.y(), rect.width(), last.height());
            painter->fillRect(lastRect, background);

            const QString remaining = paragraph.mid(last.textStart());
            const QString elided = metrics.elidedText(remaining, Qt::ElideRight, int(rect.width()));

            // The elided line has to keep its highlighting (snippet colours and
            // keyword background), otherwise narrowing the message column (for
            // example when the details pane is shown) would make the colours of a
            // truncated line disappear (REQ-HL-05).
            QTextLayout elidedLayout(elided, font);
            QTextOption elidedOption;
            elidedOption.setWrapMode(QTextOption::NoWrap);
            elidedLayout.setTextOption(elidedOption);
            elidedLayout.beginLayout();
            if (QTextLine line = elidedLayout.createLine(); line.isValid()) {
                line.setLineWidth(rect.width());
                line.setPosition(QPointF(0, 0));
            }
            elidedLayout.endLayout();

            QVector<QTextLayout::FormatRange> elidedFormats =
                shiftFormatsToElidedLine(buildFormats(formats, paragraph, paragraphStart),
                                         last.textStart(), elided.size());

            painter->save();
            painter->setFont(font);
            painter->translate(lastRect.topLeft());
            elidedLayout.draw(painter, QPointF(0, 0), elidedFormats);
            painter->restore();
        }

        y += metrics.lineSpacing() * lines.size();
        drawn += lines.size();
        paragraphStart += paragraph.size() + 1;
    }
}

} // namespace lv
