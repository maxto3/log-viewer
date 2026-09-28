#include "core/Matcher.h"

#include <QCoreApplication>

namespace lv {
namespace {

/// Word characters for the whole-word boundaries: ASCII letters, digits and '_'
/// only, so CJK terms are matched literally (REQ-FIND-08).
bool isAsciiWordCharacter(QChar c)
{
    const ushort code = c.unicode();
    return (code >= '0' && code <= '9') || (code >= 'a' && code <= 'z')
        || (code >= 'A' && code <= 'Z') || code == '_';
}

QString wholeWordPattern(const QString &pattern)
{
    const QString escaped = QRegularExpression::escape(pattern);
    const bool guardStart = !pattern.isEmpty() && isAsciiWordCharacter(pattern.front());
    const bool guardEnd = !pattern.isEmpty() && isAsciiWordCharacter(pattern.back());
    QString result = escaped;
    if (guardStart)
        result.prepend(QStringLiteral("(?<![A-Za-z0-9_])"));
    if (guardEnd)
        result.append(QStringLiteral("(?![A-Za-z0-9_])"));
    return result;
}

} // namespace

Matcher Matcher::build(const QString &pattern, MatchMode mode, bool caseSensitive,
                       QString *errorMessage)
{
    Matcher matcher;
    matcher.m_pattern = pattern;
    matcher.m_mode = mode;
    matcher.m_caseSensitive = caseSensitive;

    const QString trimmed = pattern.trimmed();
    if (trimmed.isEmpty()) {
        matcher.m_valid = true;
        return matcher;   // an empty pattern matches nothing but is not an error
    }

    if (trimmed.size() > kMaxPatternLength) {
        matcher.m_valid = false;
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate(
                                "Matcher", "The pattern is limited to %1 characters.")
                                .arg(kMaxPatternLength);
        }
        return matcher;
    }

    QString expression;
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    switch (mode) {
    case MatchMode::WholeWord:
        expression = wholeWordPattern(trimmed);
        break;
    case MatchMode::Wildcard:
        expression = QRegularExpression::wildcardToRegularExpression(
            trimmed, QRegularExpression::UnanchoredWildcardConversion);
        break;
    case MatchMode::RegularExpression:
        expression = trimmed;
        break;
    }
    if (!caseSensitive)
        options |= QRegularExpression::CaseInsensitiveOption;

    matcher.m_expression = QRegularExpression(expression, options);
    matcher.m_valid = matcher.m_expression.isValid();
    if (!matcher.m_valid && errorMessage)
        *errorMessage = matcher.m_expression.errorString();
    return matcher;
}

QString Matcher::modeName(MatchMode mode)
{
    switch (mode) {
    case MatchMode::Wildcard:
        return QCoreApplication::translate("Matcher", "Wildcard");
    case MatchMode::RegularExpression:
        return QCoreApplication::translate("Matcher", "Regular expression");
    case MatchMode::WholeWord:
        break;
    }
    return QCoreApplication::translate("Matcher", "Whole word");
}

bool Matcher::matches(const QString &text) const
{
    if (!m_valid || m_pattern.trimmed().isEmpty() || text.isEmpty())
        return false;
    return m_expression.match(text).hasMatch();
}

QVector<MatchRange> Matcher::ranges(const QString &text, int maxRanges) const
{
    QVector<MatchRange> result;
    if (!m_valid || m_pattern.trimmed().isEmpty() || text.isEmpty())
        return result;

    QRegularExpressionMatchIterator iterator = m_expression.globalMatch(text);
    while (iterator.hasNext() && result.size() < maxRanges) {
        const QRegularExpressionMatch match = iterator.next();
        if (match.capturedLength() <= 0)
            continue;
        result.append({static_cast<int>(match.capturedStart()),
                       static_cast<int>(match.capturedLength())});
    }
    return result;
}

} // namespace lv
