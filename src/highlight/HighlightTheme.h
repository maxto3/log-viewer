#pragma once

#include <QColor>
#include <QHash>
#include <QString>

namespace lv {

/// Token classes produced by the snippet tokenizer; the colours come from the
/// VSCode palettes below (design-doc §5.5).
enum class TokenKind {
    Plain = 0,
    String,
    Number,
    Constant,      // true / false / null
    Keyword,       // language keywords
    Comment,
    Key,           // JSON/YAML keys, XML attribute names
    Punctuation,
    TagName,       // XML tag names
    AttributeValue // XML attribute values
};

/// A coloured token inside a message.
struct TokenSpan {
    int start = 0;
    int length = 0;
    TokenKind kind = TokenKind::Plain;
};

/// VSCode Dark+ / Light+ token colours (spec.md REQ-HL-03).
class HighlightTheme
{
public:
    static const HighlightTheme &vscodeDark();
    static const HighlightTheme &vscodeLight();
    static const HighlightTheme &forDarkMode(bool darkMode);

    QColor color(TokenKind kind) const;
    QColor snippetBackground() const { return m_snippetBackground; }
    QColor editorForeground() const { return color(TokenKind::Plain); }
    QString name() const { return m_name; }

private:
    HighlightTheme(const QString &name, const QColor &snippetBackground,
                   const QHash<int, QColor> &colors);
    static HighlightTheme createDark();
    static HighlightTheme createLight();

    QHash<int, QColor> m_colors;
    QColor m_snippetBackground;
    QString m_name;
};

} // namespace lv
