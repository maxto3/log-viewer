#pragma once

#include <QColor>

namespace lv {

/// Terminal colour palette for ANSI SGR indices 0-15 (spec.md REQ-PARSE-12).
///
/// The values are chosen per theme so that every colour keeps at least 4.5:1
/// contrast on the table/details background (REQ-VIS-10): the same index maps
/// to different shades in the light and the dark theme, like a terminal theme.
namespace AnsiPalette {

/// Colour for palette \a index (0-7 standard, 8-15 bright) and theme.
QColor color(int index, bool dark);

} // namespace AnsiPalette

} // namespace lv
