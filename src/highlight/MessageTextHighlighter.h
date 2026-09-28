#pragma once

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
    void setFind(const Matcher &matcher, const QColor &background, const QColor &foreground);
    /// Recomputes tokens/matches for the whole message; call when content changes.
    void setContent(const QString &text);

protected:
    void highlightBlock(const QString &text) override;

private:
    const HighlightTheme *m_theme = nullptr;
    QVector<TokenSpan> m_tokens;
    QVector<MatchRange> m_matches;
    QColor m_keywordBackground = QColor(0x7C, 0xFC, 0x00);
    QColor m_keywordForeground = QColor(0x00, 0x00, 0x00);
};

} // namespace lv
