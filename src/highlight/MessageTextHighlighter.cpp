#include "highlight/MessageTextHighlighter.h"

#include "highlight/SnippetTokenizer.h"

#include <QTextBlock>
#include <QTextCharFormat>

namespace lv {

MessageTextHighlighter::MessageTextHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
    , m_theme(&HighlightTheme::forDarkMode(false))
{
}

void MessageTextHighlighter::setTheme(const HighlightTheme *theme)
{
    m_theme = theme ? theme : &HighlightTheme::forDarkMode(false);
    rehighlight();
}

void MessageTextHighlighter::setFind(const Matcher &matcher, const QColor &background,
                                     const QColor &foreground)
{
    m_keywordBackground = background;
    m_keywordForeground = foreground;
    m_matches.clear();
    if (!matcher.isEmpty() && matcher.isValid()) {
        // The whole message is matched at once; highlightBlock() maps the ranges
        // to the individual text blocks.
        const QString text = document() ? document()->toPlainText() : QString();
        m_matches = matcher.ranges(text, 2000);
    }
    rehighlight();
}

void MessageTextHighlighter::setContent(const QString &text)
{
    m_tokens = SnippetTokenizer::tokenize(text);
    m_matches.clear();
    rehighlight();
}

void MessageTextHighlighter::highlightBlock(const QString &text)
{
    if (text.isEmpty())
        return;

    const int blockStart = currentBlock().position();
    const int blockEnd = blockStart + text.size();

    for (const TokenSpan &span : m_tokens) {
        const int start = qMax(span.start, blockStart);
        const int end = qMin(span.start + span.length, blockEnd);
        if (end <= start)
            continue;
        QTextCharFormat format;
        format.setForeground(m_theme->color(span.kind));
        setFormat(start - blockStart, end - start, format);
    }

    // Keyword highlights override snippet colours (REQ-HL-07).
    for (const MatchRange &range : m_matches) {
        const int start = qMax(range.start, blockStart);
        const int end = qMin(range.start + range.length, blockEnd);
        if (end <= start)
            continue;
        QTextCharFormat format;
        format.setBackground(m_keywordBackground);
        format.setForeground(m_keywordForeground);
        setFormat(start - blockStart, end - start, format);
    }
}

} // namespace lv
