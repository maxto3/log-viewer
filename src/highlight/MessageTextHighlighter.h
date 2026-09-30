#pragma once

#include "core/AnsiText.h"
#include "core/Matcher.h"
#include "highlight/HighlightTheme.h"

#include <QColor>
#include <QSyntaxHighlighter>
#include <QVector>

namespace lv {

/// Highlights the complete message in the details pane with the same rules as
/// the table cells: VSCode colours for embedded JSON/XML/YAML snippets plus the
/// Find keyword highlight on top (spec.md REQ-HL-06).
class MessageTextHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit MessageTextHighlighter(QTextDocument *document);

    void setTheme(const HighlightTheme *theme);
    /// Selects the ANSI palette variant for terminal colours (REQ-PARSE-12).
    void setDarkTheme(bool dark);
    /// SGR styles of the currently shown message; set before setContent().
    void setAnsiSpans(const QVector<AnsiSpan> &spans);
    void setFind(const Matcher &matcher, const QColor &background, const QColor &foreground);
    /// Recomputes tokens/matches for the whole message; call when content changes.
    void setContent(const QString &text);

protected:
    void highlightBlock(const QString &text) override;

private:
    const HighlightTheme *m_theme = nullptr;
    QVector<TokenSpan> m_tokens;
    QVector<AnsiSpan> m_ansiSpans;
    QVector<MatchRange> m_matches;
    bool m_dark = false;
    QColor m_keywordBackground = QColor(0x7C, 0xFC, 0x00);
    QColor m_keywordForeground = QColor(0x00, 0x00, 0x00);
};

} // namespace lv
