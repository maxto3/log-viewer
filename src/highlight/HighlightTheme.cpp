#include "highlight/HighlightTheme.h"

namespace lv {

HighlightTheme::HighlightTheme(const QString &name, const QColor &snippetBackground,
                               const QHash<int, QColor> &colors)
    : m_colors(colors)
    , m_snippetBackground(snippetBackground)
    , m_name(name)
{
}

HighlightTheme HighlightTheme::createDark()
{
    return HighlightTheme(QStringLiteral("dark+"), QColor(0x1E, 0x1E, 0x1E),
                          {
                              {static_cast<int>(TokenKind::Plain), QColor(0xD4, 0xD4, 0xD4)},
                              {static_cast<int>(TokenKind::String), QColor(0xCE, 0x91, 0x78)},
                              {static_cast<int>(TokenKind::Number), QColor(0xB5, 0xCE, 0xA8)},
                              {static_cast<int>(TokenKind::Constant), QColor(0x56, 0x9C, 0xD6)},
                              {static_cast<int>(TokenKind::Keyword), QColor(0xC5, 0x86, 0xC0)},
                              {static_cast<int>(TokenKind::Comment), QColor(0x6A, 0x99, 0x55)},
                              {static_cast<int>(TokenKind::Key), QColor(0x9C, 0xDC, 0xFE)},
                              {static_cast<int>(TokenKind::Punctuation), QColor(0x80, 0x80, 0x80)},
                              {static_cast<int>(TokenKind::TagName), QColor(0x56, 0x9C, 0xD6)},
                              {static_cast<int>(TokenKind::AttributeValue), QColor(0xCE, 0x91, 0x78)},
                          });
}

HighlightTheme HighlightTheme::createLight()
{
    return HighlightTheme(QStringLiteral("light+"), QColor(0xFF, 0xFF, 0xFF),
                          {
                              {static_cast<int>(TokenKind::Plain), QColor(0x00, 0x00, 0x00)},
                              {static_cast<int>(TokenKind::String), QColor(0xA3, 0x15, 0x15)},
                              {static_cast<int>(TokenKind::Number), QColor(0x09, 0x86, 0x58)},
                              {static_cast<int>(TokenKind::Constant), QColor(0x00, 0x00, 0xFF)},
                              {static_cast<int>(TokenKind::Keyword), QColor(0xAF, 0x00, 0xDB)},
                              {static_cast<int>(TokenKind::Comment), QColor(0x00, 0x80, 0x00)},
                              {static_cast<int>(TokenKind::Key), QColor(0x00, 0x10, 0x80)},
                              {static_cast<int>(TokenKind::Punctuation), QColor(0x38, 0x38, 0x38)},
                              {static_cast<int>(TokenKind::TagName), QColor(0x80, 0x00, 0x00)},
                              {static_cast<int>(TokenKind::AttributeValue), QColor(0x00, 0x00, 0xFF)},
                          });
}

const HighlightTheme &HighlightTheme::vscodeDark()
{
    static const HighlightTheme theme = createDark();
    return theme;
}

const HighlightTheme &HighlightTheme::vscodeLight()
{
    static const HighlightTheme theme = createLight();
    return theme;
}

const HighlightTheme &HighlightTheme::forDarkMode(bool darkMode)
{
    return darkMode ? vscodeDark() : vscodeLight();
}

QColor HighlightTheme::color(TokenKind kind) const
{
    const auto it = m_colors.constFind(static_cast<int>(kind));
    if (it != m_colors.constEnd())
        return *it;
    return m_colors.value(static_cast<int>(TokenKind::Plain));
}

} // namespace lv
