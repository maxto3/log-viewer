#include "core/DurationFormat.h"

#include <QCoreApplication>

namespace lv {
namespace {

QString translated(const char *source)
{
    return QCoreApplication::translate("DurationFormat", source);
}

} // namespace

QString formatDuration(qint64 milliseconds)
{
    if (milliseconds < 0)
        milliseconds = 0;

    // The branch is chosen from the rounded value so that 59.96 s shows as
    // "1 min" instead of a rounded-up "60.0 s".
    const qint64 totalSeconds = (milliseconds + 500) / 1000;
    if (totalSeconds < 60) {
        // Sub-minute durations keep sub-second precision so that fast opens
        // remain distinguishable: "0.35 s", "5.32 s", "12.3 s".
        const double seconds = milliseconds / 1000.0;
        const int decimals = seconds < 10.0 ? 2 : 1;
        return translated("%1 s").arg(QString::number(seconds, 'f', decimals));
    }

    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        if (minutes > 0 && seconds > 0)
            return translated("%1 h %2 min %3 s").arg(hours).arg(minutes).arg(seconds);
        if (minutes > 0)
            return translated("%1 h %2 min").arg(hours).arg(minutes);
        if (seconds > 0)
            return translated("%1 h %2 s").arg(hours).arg(seconds);
        return translated("%1 h").arg(hours);
    }
    if (seconds > 0)
        return translated("%1 min %2 s").arg(minutes).arg(seconds);
    return translated("%1 min").arg(minutes);
}

} // namespace lv
