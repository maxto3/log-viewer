#include "highlight/AnsiPalette.h"

#include <array>

namespace lv {
namespace {

// Light theme: readable on #FFFFFF (worst contrast 5.13:1).
constexpr std::array<const char *, 16> kLight = {
    "#202020", "#C62828", "#2E7D32", "#7A5C00", "#0451A5", "#BC05BC", "#00707F", "#555555",
    "#6A6A6A", "#B71C1C", "#1B5E20", "#6D4C00", "#0A4A8A", "#8E24AA", "#006064", "#6A6A6A"};

// Dark theme: readable on #1E1E1E (worst contrast 5.22:1).
constexpr std::array<const char *, 16> kDark = {
    "#909090", "#FF6B6B", "#4EC9B0", "#E5E510", "#569CD6", "#D670D6", "#29B8DB", "#E5E5E5",
    "#B0B0B0", "#FF8A8A", "#6FE0C8", "#F5F543", "#7AB8F5", "#E39BE3", "#5FD3EF", "#FFFFFF"};

} // namespace

QColor AnsiPalette::color(int index, bool dark)
{
    if (index < 0 || index > 15)
        return QColor();
    const char *name = dark ? kDark[static_cast<size_t>(index)] : kLight[static_cast<size_t>(index)];
    return QColor(QLatin1String(name));
}

} // namespace lv
