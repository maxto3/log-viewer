#include "highlight/LevelPalette.h"

namespace lv {
namespace {

struct Pair {
    const char *text;
    const char *background;
};

// Order matches LogLevel: Trace, Debug, Info, Notice, Warn, Error, Fatal, Other.
const Pair kLight[] = {
    {"#6B7280", "#F1F3F4"},
    {"#3F6212", "#F0F5E5"},
    {"#1565C0", "#E8F1FB"},
    {"#00695C", "#E0F2F1"},
    {"#B26A00", "#FFF4E5"},
    {"#C62828", "#FDECEA"},
    {"#8E0000", "#F9DEDE"},
    {"#455A64", "#ECEFF1"},
};

const Pair kDark[] = {
    {"#808080", "#2D2D30"},
    {"#6A9955", "#26331F"},
    {"#3794FF", "#13273D"},
    {"#4EC9B0", "#0F2A26"},
    {"#CCA700", "#332C14"},
    {"#F14C4C", "#3A1D1D"},
    {"#FF6B6B", "#40201F"},
    {"#9E9E9E", "#2D2D30"},
};

} // namespace

LevelColors levelColors(LogLevel level, bool darkTheme)
{
    const int index = logLevelIndex(level);
    const Pair &pair = darkTheme ? kDark[index] : kLight[index];
    return {QColor(QLatin1StringView(pair.text)), QColor(QLatin1StringView(pair.background))};
}

QColor timestampColor(bool darkTheme)
{
    return QColor(darkTheme ? QLatin1StringView("#808080") : QLatin1StringView("#6A737D"));
}

} // namespace lv
