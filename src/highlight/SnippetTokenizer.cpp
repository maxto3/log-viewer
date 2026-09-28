#include "highlight/SnippetTokenizer.h"

#include <QRegularExpression>

namespace lv {
namespace {

bool isJsonCandidateStart(QChar c)
{
    return c == u'{' || c == u'[';
}

bool isNumberStart(QChar c)
{
    return c.isDigit() || c == u'-' || c == u'+';
}

bool isIdentifierChar(QChar c)
{
    return c.isLetterOrNumber() || c == u'_' || c == u'-' || c == u'.';
}

/// Finds the end of a JSON object/array starting at \a start; returns -1 when it
/// is not closed (a truncated snippet is not highlighted).
int jsonRangeEnd(const QString &text, int start)
{
    Q_ASSERT(start < text.size() && isJsonCandidateStart(text.at(start)));
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (int i = start; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (inString) {
            if (escaped)
                escaped = false;
            else if (c == u'\\')
                escaped = true;
            else if (c == u'"')
                inString = false;
            continue;
        }
        if (c == u'"') {
            inString = true;
        } else if (c == u'{' || c == u'[') {
            ++depth;
        } else if (c == u'}' || c == u']') {
            --depth;
            if (depth == 0)
                return i + 1;
        }
    }
    return -1;
}

/// Finds the end of a balanced XML element starting at \a start (pointing at '<').
int xmlRangeEnd(const QString &text, int start)
{
    int depth = 0;
    int i = start;
    while (i < text.size()) {
        const int open = text.indexOf(QLatin1Char('<'), i);
        if (open < 0)
            return -1;
        if (text.mid(open, 4) == QLatin1String("<!--")) {
            const int close = text.indexOf(QLatin1String("-->"), open + 4);
            if (close < 0)
                return -1;
            i = close + 3;
            continue;
        }
        const bool isClosing = open + 1 < text.size() && text.at(open + 1) == u'/';
        const int tagEnd = text.indexOf(QLatin1Char('>'), open);
        if (tagEnd < 0)
            return -1;
        const bool selfClosing = text.at(tagEnd - 1) == u'/';
        if (!isClosing && !selfClosing)
            ++depth;
        if (isClosing) {
            --depth;
            if (depth == 0)
                return tagEnd + 1;
        }
        if (depth < 0)
            return -1;
        i = tagEnd + 1;
    }
    return -1;
}

bool looksLikeYamlLine(const QString &line, const QString &next)
{
    static const QRegularExpression keyPattern(
        QStringLiteral(R"(^\s*(?:-\s+)?[A-Za-z_][A-Za-z0-9_.\-]*\s*:(?:\s|$))"));
    if (keyPattern.match(line).hasMatch())
        return true;
    // A list entry whose content is another mapping or a scalar.
    static const QRegularExpression listPattern(QStringLiteral(R"(^\s*-\s+\S)"));
    return listPattern.match(line).hasMatch()
        && (keyPattern.match(next).hasMatch() || line.trimmed().size() > 3);
}

} // namespace

QVector<TokenSpan> SnippetTokenizer::tokenize(const QString &text)
{
    QVector<TokenSpan> spans;
    if (text.size() < kMinSnippetLength)
        return spans;

    int budget = kMaxSnippets;
    int cursor = 0;
    while (cursor < text.size() && budget > 0) {
        const QChar c = text.at(cursor);
        if (isJsonCandidateStart(c)) {
            int before = spans.size();
            tokenizeJsonSnippets(text, cursor, spans, &budget);
            if (spans.size() > before)
                cursor = spans.last().start + spans.last().length;
        } else if (c == u'<') {
            int before = spans.size();
            tokenizeXmlSnippets(text, cursor, spans, &budget);
            if (spans.size() > before)
                cursor = spans.last().start + spans.last().length;
        }
        ++cursor;
    }

    int before = spans.size();
    tokenizeYamlSnippets(text, 0, spans, &budget);
    Q_UNUSED(before);

    std::sort(spans.begin(), spans.end(), [](const TokenSpan &a, const TokenSpan &b) {
        return a.start < b.start;
    });
    return spans;
}

void SnippetTokenizer::tokenizeJsonSnippets(const QString &text, int from, QVector<TokenSpan> &spans,
                                            int *budget)
{
    const int end = jsonRangeEnd(text, from);
    if (end < 0)
        return;
    const QString snippet = text.mid(from, end - from);
    if (snippet.size() < kMinSnippetLength || snippet.size() > kMaxSnippetChars)
        return;
    // A single brace or a plain "[1,2]" is too weak to be called JSON.
    if (!snippet.contains(QLatin1Char(':')) && !snippet.contains(QLatin1Char(',')))
        return;

    QVector<TokenSpan> local;
    tokenizeJsonRange(text, from, end - from, local);
    if (local.isEmpty())
        return;
    spans += local;
    --(*budget);
}

void SnippetTokenizer::tokenizeXmlSnippets(const QString &text, int from, QVector<TokenSpan> &spans,
                                           int *budget)
{
    if (from + 1 >= text.size())
        return;
    const QChar next = text.at(from + 1);
    if (!(next.isLetter() || next == u'?' || next == u'!'))
        return;
    const int end = xmlRangeEnd(text, from);
    if (end < 0)
        return;
    const QString snippet = text.mid(from, end - from);
    if (snippet.size() < kMinSnippetLength || snippet.size() > kMaxSnippetChars)
        return;
    if (!snippet.contains(QLatin1String("</")) && !snippet.endsWith(QLatin1String("/>")))
        return;
    if (snippet.count(QLatin1Char('<')) < 2)
        return;

    QVector<TokenSpan> local;
    tokenizeXmlRange(text, from, end - from, local);
    if (local.isEmpty())
        return;
    spans += local;
    --(*budget);
}

void SnippetTokenizer::tokenizeYamlSnippets(const QString &text, int from, QVector<TokenSpan> &spans,
                                            int *budget)
{
    if (*budget <= 0)
        return;

    const QStringList lines = text.mid(from).split(QLatin1Char('\n'));
    int blockStart = -1;
    int blockLines = 0;
    int offset = from;

    const auto flush = [&](int endOffset) {
        if (blockLines >= 2 && blockStart >= 0) {
            const int length = endOffset - blockStart;
            if (length >= kMinSnippetLength && length <= kMaxSnippetChars) {
                QVector<TokenSpan> local;
                tokenizeYamlRange(text, blockStart, length, local);
                if (!local.isEmpty() && *budget > 0) {
                    spans += local;
                    --(*budget);
                }
            }
        }
        blockStart = -1;
        blockLines = 0;
    };

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        const QString next = i + 1 < lines.size() ? lines.at(i + 1) : QString();
        const bool yaml = looksLikeYamlLine(line, next);
        if (yaml) {
            if (blockStart < 0)
                blockStart = offset;
            ++blockLines;
        } else if (!line.trimmed().isEmpty()) {
            flush(offset);
        } else {
            flush(offset);
        }
        offset += line.size() + 1;
    }
    flush(text.size());
}

void SnippetTokenizer::tokenizeJsonRange(const QString &text, int start, int length,
                                         QVector<TokenSpan> &spans)
{
    int i = start;
    const int end = start + length;
    while (i < end) {
        const QChar c = text.at(i);
        if (c == u'"') {
            int j = i + 1;
            bool escaped = false;
            while (j < end) {
                const QChar inner = text.at(j);
                if (escaped) {
                    escaped = false;
                } else if (inner == u'\\') {
                    escaped = true;
                } else if (inner == u'"') {
                    break;
                }
                ++j;
            }
            const int stringEnd = qMin(j + 1, end);
            // A string followed by ':' is a key.
            int k = stringEnd;
            while (k < end && text.at(k).isSpace())
                ++k;
            const bool isKey = k < end && text.at(k) == u':';
            spans.append({i, stringEnd - i, isKey ? TokenKind::Key : TokenKind::String});
            i = stringEnd;
            continue;
        }
        if (isNumberStart(c)) {
            int j = i;
            while (j < end && (text.at(j).isDigit() || text.at(j) == u'.' || text.at(j) == u'-'
                               || text.at(j) == u'+' || text.at(j) == u'e' || text.at(j) == u'E')) {
                ++j;
            }
            spans.append({i, j - i, TokenKind::Number});
            i = j;
            continue;
        }
        if (c.isLetter()) {
            int j = i;
            while (j < end && text.at(j).isLetter())
                ++j;
            const QString word = text.mid(i, j - i);
            if (word == QLatin1String("true") || word == QLatin1String("false")
                || word == QLatin1String("null")) {
                spans.append({i, j - i, TokenKind::Constant});
            }
            i = j;
            continue;
        }
        if (c == u'{' || c == u'}' || c == u'[' || c == u']' || c == u':' || c == u',') {
            spans.append({i, 1, TokenKind::Punctuation});
        }
        ++i;
    }
}

void SnippetTokenizer::tokenizeXmlRange(const QString &text, int start, int length,
                                        QVector<TokenSpan> &spans)
{
    int i = start;
    const int end = start + length;
    while (i < end) {
        const QChar c = text.at(i);
        if (c != u'<') {
            ++i;
            continue;
        }
        if (text.mid(i, 4) == QLatin1String("<!--")) {
            const int close = text.indexOf(QLatin1String("-->"), i + 4);
            const int commentEnd = close < 0 ? end : qMin(close + 3, end);
            spans.append({i, commentEnd - i, TokenKind::Comment});
            i = commentEnd;
            continue;
        }
        // Tag: <[/?]name attr="value" ...>
        int j = i + 1;
        if (j < end && (text.at(j) == u'/' || text.at(j) == u'?'))
            ++j;
        const int nameStart = j;
        while (j < end && (isIdentifierChar(text.at(j)) || text.at(j) == u':'))
            ++j;
        if (j > nameStart)
            spans.append({nameStart, j - nameStart, TokenKind::TagName});
        spans.append({i, 1, TokenKind::Punctuation});

        while (j < end && text.at(j) != u'>') {
            const QChar attr = text.at(j);
            if (attr == u'/' || attr == u'?' || attr.isSpace()) {
                ++j;
                continue;
            }
            if (attr == u'=') {
                spans.append({j, 1, TokenKind::Punctuation});
                ++j;
                continue;
            }
            if (attr == u'"' || attr == u'\'') {
                const QChar quote = attr;
                int k = j + 1;
                while (k < end && text.at(k) != quote)
                    ++k;
                const int valueEnd = qMin(k + 1, end);
                spans.append({j, valueEnd - j, TokenKind::AttributeValue});
                j = valueEnd;
                continue;
            }
            int k = j;
            while (k < end && text.at(k) != u'=' && text.at(k) != u'>' && !text.at(k).isSpace())
                ++k;
            spans.append({j, k - j, TokenKind::Key});
            j = k;
        }
        if (j < end && text.at(j) == u'>')
            spans.append({j, 1, TokenKind::Punctuation});
        i = j + 1;
    }
}

void SnippetTokenizer::tokenizeYamlRange(const QString &text, int start, int length,
                                         QVector<TokenSpan> &spans)
{
    int offset = start;
    const int end = start + length;
    while (offset < end) {
        int lineEnd = text.indexOf(QLatin1Char('\n'), offset);
        if (lineEnd < 0 || lineEnd > end)
            lineEnd = end;
        const QString line = text.mid(offset, lineEnd - offset);

        static const QRegularExpression keyPattern(
            QStringLiteral(R"(^(\s*)(-\s+)?([A-Za-z_][A-Za-z0-9_.\-]*)(\s*):)"));
        const QRegularExpressionMatch match = keyPattern.match(line);
        int valueStart = 0;
        if (match.hasMatch()) {
            const int indent = static_cast<int>(match.capturedLength(1));
            const int dashStart = indent + static_cast<int>(match.capturedLength(2)) - 2;
            if (match.capturedLength(2) > 0)
                spans.append({offset + dashStart, 1, TokenKind::Punctuation});
            spans.append({offset + static_cast<int>(match.capturedStart(3)), static_cast<int>(match.capturedLength(3)), TokenKind::Key});
            spans.append({offset + static_cast<int>(match.capturedStart(4)), 1, TokenKind::Punctuation});
            valueStart = static_cast<int>(match.capturedEnd(0));
        } else {
            static const QRegularExpression listPattern(QStringLiteral(R"(^(\s*)(-\s+))"));
            const QRegularExpressionMatch listMatch = listPattern.match(line);
            if (listMatch.hasMatch()) {
                spans.append({offset + static_cast<int>(listMatch.capturedStart(2)), 1, TokenKind::Punctuation});
                valueStart = static_cast<int>(listMatch.capturedEnd(0));
            }
        }

        // Value: string / number / boolean / null, comments.
        int i = valueStart;
        while (i < line.size()) {
            const QChar c = line.at(i);
            if (c == u'#') {
                spans.append({offset + i, static_cast<int>(line.size()) - i, TokenKind::Comment});
                break;
            }
            if (c.isSpace()) {
                ++i;
                continue;
            }
            int j = i;
            while (j < line.size() && !line.at(j).isSpace() && line.at(j) != u'#')
                ++j;
            const QString value = line.mid(i, j - i);
            TokenKind kind = TokenKind::String;
            if (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\'')))
                kind = TokenKind::String;
            else if (isNumberStart(value.at(0)) && value.at(value.size() - 1).isDigit())
                kind = TokenKind::Number;
            else if (value == QLatin1String("true") || value == QLatin1String("false")
                     || value == QLatin1String("null") || value == QLatin1String("~"))
                kind = TokenKind::Constant;
            spans.append({offset + i, static_cast<int>(value.size()), kind});
            i = j;
        }
        offset = lineEnd + 1;
    }
}

} // namespace lv
