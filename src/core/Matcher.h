#pragma once

#include <QRegularExpression>
#include <QString>
#include <QVector>

namespace lv {

/// Match strategy of the Find / Filter boxes (spec.md REQ-FIND-02).
enum class MatchMode {
    WholeWord = 0,
    Wildcard = 1,
    RegularExpression = 2
};

/// A match inside a text, used for highlighting.
struct MatchRange {
    int start = 0;
    int length = 0;
};

/// Compiled Find / Filter pattern.
///
///  * whole word: the pattern is treated literally and must be delimited by
///    non-word characters (CJK characters are not word characters, so Chinese
///    search terms still match inside a Chinese sentence, REQ-FIND-08);
///  * wildcard: '*' and '?' expand to a regular expression;
///  * regular expression: PCRE2 as provided by Qt, pattern length capped at 512
///    characters (OPEN-06 protection against pathological patterns).
class Matcher
{
public:
    static constexpr int kMaxPatternLength = 512;

    /// Compiles \a pattern; an invalid pattern leaves the matcher invalid and
    /// fills \a errorMessage.
    static Matcher build(const QString &pattern, MatchMode mode, bool caseSensitive,
                         QString *errorMessage = nullptr);

    static QString modeName(MatchMode mode);

    bool isEmpty() const { return m_pattern.isEmpty(); }
    bool isValid() const { return m_valid; }
    const QString &pattern() const { return m_pattern; }
    MatchMode mode() const { return m_mode; }
    bool caseSensitive() const { return m_caseSensitive; }

    /// True when \a text contains at least one match.
    bool matches(const QString &text) const;

    /// All matches in \a text (capped at \a maxRanges).
    QVector<MatchRange> ranges(const QString &text, int maxRanges = 200) const;

private:
    QString m_pattern;
    MatchMode m_mode = MatchMode::WholeWord;
    bool m_caseSensitive = false;
    bool m_valid = true;
    QRegularExpression m_expression;
};

} // namespace lv
