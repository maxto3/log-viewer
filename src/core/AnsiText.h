#pragma once

#include <QColor>
#include <QString>
#include <QVector>

namespace lv {

/// Terminal style carried by an ANSI SGR span (spec.md REQ-PARSE-12).
struct AnsiStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeOut = false;
    /// Palette index 0-15 (standard/bright); -1 when unset.
    int foreground = -1;
    int background = -1;
    /// 256-colour / truecolor values (used when the matching "has" flag is set).
    QColor rgbForeground;
    QColor rgbBackground;
    bool hasRgbForeground = false;
    bool hasRgbBackground = false;

    bool operator==(const AnsiStyle &other) const;
    /// True when no SGR attribute is active (plain text).
    bool isDefault() const;
};

/// A styled range inside the cleaned text produced by AnsiText::process().
struct AnsiSpan {
    int start = 0;
    int length = 0;
    AnsiStyle style;
};

/// Cleaned display text plus the SGR styles that were active around it.
struct AnsiTextResult {
    QString text;
    QVector<AnsiSpan> spans;
    bool hadEscapes = false;
};

/// ANSI escape sequence handling (spec.md REQ-PARSE-12): control sequences are
/// removed, SGR attributes are reported as spans of the cleaned text so the UI
/// can render terminal colours.
namespace AnsiText {

/// Removes ANSI escape sequences and C0/C1 control characters (TAB and LF are
/// kept). Fast path: returns \a input unchanged when it has no ESC/controls.
QString strip(const QString &input);

/// Like strip(), but also returns the SGR spans over the cleaned text.
AnsiTextResult process(const QString &input);

} // namespace AnsiText

} // namespace lv
