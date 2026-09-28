#pragma once

#include "core/LogLevel.h"

#include <QColor>

namespace lv {

/// Text and background colours of a level chip.
struct LevelColors {
    QColor text;
    QColor background;
};

/// Colours follow the VSCode inspired palette documented in design-doc §5.2.
LevelColors levelColors(LogLevel level, bool darkTheme);

/// Muted colour used for timestamps and other secondary text.
QColor timestampColor(bool darkTheme);

} // namespace lv
