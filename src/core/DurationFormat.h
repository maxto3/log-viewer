#pragma once

#include <QString>
#include <QtGlobal>

namespace lv {

/// Formats a duration for the status bar (spec.md REQ-UI-15). Sub-minute
/// values keep sub-second precision ("0.35 s", "12.3 s"), longer ones use
/// minutes and seconds ("2 min 5 s") and hours, minutes and seconds
/// ("1 h 2 min 5 s"); zero-valued leading components are omitted ("1 h 5 s").
QString formatDuration(qint64 milliseconds);

} // namespace lv
