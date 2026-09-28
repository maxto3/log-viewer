#pragma once

#include "core/ColumnKind.h"
#include "core/Matcher.h"
#include "highlight/HighlightTheme.h"

#include <QColor>
#include <QFont>
#include <QStyledItemDelegate>
#include <QTextLayout>

#include <functional>

namespace lv {

class LogTableModel;

/// Custom cell painter:
///  * two line rows with a trailing "…" when the content does not fit;
///  * coloured level chips and muted timestamps;
///  * keyword highlights (Find) on top of VSCode coloured JSON/XML/YAML
///    snippet tokens (spec.md REQ-HL-01…07);
///  * full height rows when a row is expanded or the full content mode is on.
class LogItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// Maximum number of lines drawn for an expanded row.
    static constexpr int kExpandedLineLimit = 30;
    /// Safety cap for the full content mode (a single entry never renders more).
    static constexpr int kFullContentLineLimit = 200;

    explicit LogItemDelegate(QObject *parent = nullptr);

    void setDarkTheme(bool dark);
    void setCellFont(const QFont &font);
    void setRowHeightLines(int lines);
    /// When enabled every row is drawn with its complete content (no two line
    /// clamp, no ellipsis) and the view sizes the rows accordingly.
    void setFullContentMode(bool enabled);
    bool fullContentMode() const { return m_fullContent; }
    /// Height of a row when it is not expanded (independent of the content).
    int defaultRowHeight() const;
    /// Height needed to draw the complete content of \a index in \a width pixels.
    int contentRowHeight(const QModelIndex &index, int width) const;

    /// VSCode palette used for embedded JSON/XML/YAML snippets.
    void setHighlightTheme(const HighlightTheme *theme);
    /// Colours of the Find highlight (default: green background, black text).
    void setKeywordColors(const QColor &background, const QColor &foreground);

    /// The view tells the delegate which rows are currently expanded.
    void setExpansionProbe(std::function<bool(int row)> probe);

    /// Maps paragraph-relative format ranges onto the elided text of the last
    /// visible line. The elided line must keep its highlighting, otherwise
    /// narrowing the message column (details pane shown) would drop the colours
    /// of every truncated line (REQ-HL-05). Public so the regression test can
    /// verify the mapping.
    static QVector<QTextLayout::FormatRange> shiftFormatsToElidedLine(
        const QVector<QTextLayout::FormatRange> &paragraphFormats,
        int lineStartInParagraph, int elidedLength);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    /// Everything needed to colour one cell.
    struct CellFormats {
        const QVector<TokenSpan> *tokens = nullptr;
        const HighlightTheme *theme = nullptr;
        QVector<MatchRange> keywords;
        QColor keywordBackground;
        QColor keywordForeground;
    };

    QFont cellFont() const;
    int lineCountForRow(int row) const;
    static void drawClampedText(QPainter *painter, const QRect &rect, const QString &text,
                                int maxLines, const QColor &background, const CellFormats &formats);
    static QVector<QTextLayout::FormatRange> buildFormats(const CellFormats &formats,
                                                          const QString &paragraph, int paragraphStart);

    bool m_dark = false;
    bool m_fullContent = false;
    int m_rowHeightLines = 2;
    QFont m_cellFont;
    const HighlightTheme *m_theme = nullptr;
    QColor m_keywordBackground = QColor(0x7C, 0xFC, 0x00);
    QColor m_keywordForeground = QColor(0x00, 0x00, 0x00);
    std::function<bool(int)> m_isExpanded;
};

} // namespace lv
