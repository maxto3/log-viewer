#include "core/AnsiText.h"

#include <QList>
#include <QStringList>

namespace lv {
namespace {

bool isCsiFinal(ushort value)
{
    return value >= 0x40 && value <= 0x7E;
}

QVector<int> parseSgrParameters(const QString &parameters)
{
    // ESC[m (empty) is a reset, and empty/malformed entries count as 0.
    const QStringList parts = parameters.split(QLatin1Char(';'), Qt::KeepEmptyParts);
    QVector<int> values;
    values.reserve(parts.size());
    for (const QString &part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        values.append(ok ? value : 0);
    }
    return values;
}

QColor xterm256Color(int index)
{
    static constexpr int kSteps[6] = {0, 95, 135, 175, 215, 255};
    if (index < 232) {
        const int cube = index - 16;
        return QColor(kSteps[cube / 36], kSteps[(cube / 6) % 6], kSteps[cube % 6]);
    }
    const int gray = 8 + (index - 232) * 10;
    return QColor(gray, gray, gray);
}

void applyExtendedColor(const QVector<int> &parameters, int &position, bool foreground,
                        AnsiStyle &style)
{
    // position points at 38/48; the forms are 38;5;n and 38;2;r;g;b.
    if (position + 2 < parameters.size() && parameters.at(position + 1) == 5) {
        const int index = parameters.at(position + 2);
        position += 2;
        if (index >= 0 && index < 16) {
            if (foreground) {
                style.foreground = index;
                style.hasRgbForeground = false;
            } else {
                style.background = index;
                style.hasRgbBackground = false;
            }
        } else if (index >= 16 && index <= 255) {
            const QColor color = xterm256Color(index);
            if (foreground) {
                style.rgbForeground = color;
                style.hasRgbForeground = true;
                style.foreground = -1;
            } else {
                style.rgbBackground = color;
                style.hasRgbBackground = true;
                style.background = -1;
            }
        }
        return;
    }
    if (position + 4 < parameters.size() && parameters.at(position + 1) == 2) {
        const QColor color(qBound(0, parameters.at(position + 2), 255),
                           qBound(0, parameters.at(position + 3), 255),
                           qBound(0, parameters.at(position + 4), 255));
        position += 4;
        if (foreground) {
            style.rgbForeground = color;
            style.hasRgbForeground = true;
            style.foreground = -1;
        } else {
            style.rgbBackground = color;
            style.hasRgbBackground = true;
            style.background = -1;
        }
    }
}

void applySgr(const QVector<int> &parameters, AnsiStyle &style)
{
    for (int i = 0; i < parameters.size(); ++i) {
        const int value = parameters.at(i);
        switch (value) {
        case 0:
            style = AnsiStyle();
            break;
        case 1:
            style.bold = true;
            break;
        case 3:
            style.italic = true;
            break;
        case 4:
            style.underline = true;
            break;
        case 9:
            style.strikeOut = true;
            break;
        case 22:
            style.bold = false;
            break;
        case 23:
            style.italic = false;
            break;
        case 24:
            style.underline = false;
            break;
        case 29:
            style.strikeOut = false;
            break;
        case 39:
            style.foreground = -1;
            style.hasRgbForeground = false;
            break;
        case 49:
            style.background = -1;
            style.hasRgbBackground = false;
            break;
        case 38:
        case 48:
            applyExtendedColor(parameters, i, value == 38, style);
            break;
        default:
            if (value >= 30 && value <= 37) {
                style.foreground = value - 30;
                style.hasRgbForeground = false;
            } else if (value >= 40 && value <= 47) {
                style.background = value - 40;
                style.hasRgbBackground = false;
            } else if (value >= 90 && value <= 97) {
                style.foreground = 8 + value - 90;
                style.hasRgbForeground = false;
            } else if (value >= 100 && value <= 107) {
                style.background = 8 + value - 100;
                style.hasRgbBackground = false;
            }
            break;
        }
    }
}

/// Returns the index after the ESC sequence starting at \a escapeIndex (which
/// points at the ESC itself): CSI, OSC/DCS/SOS/PM/APC and two-character
/// escapes (e.g. charset selection ESC ( B).
int skipEscapeSequence(const QString &input, int escapeIndex)
{
    const int size = input.size();
    int i = escapeIndex + 1;
    if (i >= size)
        return i;

    const QChar next = input.at(i);
    if (next.unicode() == '[') {
        ++i;
        while (i < size && !isCsiFinal(input.at(i).unicode()))
            ++i;
        return i < size ? i + 1 : size;
    }
    if (next.unicode() == ']' || next.unicode() == 'P' || next.unicode() == 'X'
        || next.unicode() == '^' || next.unicode() == '_') {
        ++i;
        while (i < size) {
            if (input.at(i).unicode() == 0x07)     // BEL
                return i + 1;
            if (input.at(i).unicode() == 0x1B && i + 1 < size
                && input.at(i + 1).unicode() == '\\') {
                return i + 2;                      // ST
            }
            ++i;
        }
        return size;
    }
    if (next.unicode() >= 0x20 && next.unicode() <= 0x2F) {
        ++i;
        while (i < size && input.at(i).unicode() >= 0x20 && input.at(i).unicode() <= 0x2F)
            ++i;
        if (i < size && input.at(i).unicode() >= 0x30 && input.at(i).unicode() <= 0x7E)
            ++i;
        return i;
    }
    return i + 1;
}

bool hasControlCharacter(const QString &input)
{
    for (const QChar c : input) {
        const ushort value = c.unicode();
        if (value == 0x1B)
            return true;
        if (value < 0x20 && c != QLatin1Char('\t') && c != QLatin1Char('\n'))
            return true;
        if (value == 0x7F || (value >= 0x80 && value <= 0x9F))
            return true;
    }
    return false;
}

} // namespace

bool AnsiStyle::operator==(const AnsiStyle &other) const
{
    return bold == other.bold && italic == other.italic && underline == other.underline
        && strikeOut == other.strikeOut && foreground == other.foreground
        && background == other.background && hasRgbForeground == other.hasRgbForeground
        && hasRgbBackground == other.hasRgbBackground
        && (!hasRgbForeground || rgbForeground == other.rgbForeground)
        && (!hasRgbBackground || rgbBackground == other.rgbBackground);
}

bool AnsiStyle::isDefault() const
{
    return !bold && !italic && !underline && !strikeOut && foreground < 0 && background < 0
        && !hasRgbForeground && !hasRgbBackground;
}

QString AnsiText::strip(const QString &input)
{
    if (!hasControlCharacter(input))
        return input;
    return process(input).text;
}

AnsiTextResult AnsiText::process(const QString &input)
{
    AnsiTextResult result;
    if (input.isEmpty())
        return result;
    result.text.reserve(input.size());

    AnsiStyle style;
    int spanStart = -1;
    int spanLength = 0;
    AnsiStyle spanStyle;

    const auto flushSpan = [&] {
        if (spanStart >= 0 && spanLength > 0)
            result.spans.append({spanStart, spanLength, spanStyle});
        spanStart = -1;
        spanLength = 0;
    };
    const auto appendCharacter = [&](QChar character) {
        if (!style.isDefault()) {
            if (spanStart >= 0 && !(spanStyle == style))
                flushSpan();
            if (spanStart < 0) {
                spanStart = result.text.size();
                spanStyle = style;
            }
            ++spanLength;
        } else {
            flushSpan();
        }
        result.text.append(character);
    };

    int i = 0;
    while (i < input.size()) {
        const QChar c = input.at(i);
        if (c.unicode() == 0x1B) {
            result.hadEscapes = true;
            flushSpan();
            // SGR is the only sequence with a visual effect.
            const int cursor = i + 1;
            if (cursor < input.size() && input.at(cursor).unicode() == '[') {
                int end = cursor + 1;
                while (end < input.size() && !isCsiFinal(input.at(end).unicode()))
                    ++end;
                if (end < input.size() && input.at(end).unicode() == 'm') {
                    applySgr(parseSgrParameters(input.mid(cursor + 1, end - cursor - 1)), style);
                }
            }
            i = skipEscapeSequence(input, i);
            continue;
        }
        if (c == QLatin1Char('\t') || c == QLatin1Char('\n')) {
            appendCharacter(c);
            ++i;
            continue;
        }
        const ushort value = c.unicode();
        if (value < 0x20 || value == 0x7F || (value >= 0x80 && value <= 0x9F)) {
            // Unprintable control characters never reach the UI (REQ-PARSE-12).
            flushSpan();
            ++i;
            continue;
        }
        appendCharacter(c);
        ++i;
    }
    flushSpan();
    return result;
}

} // namespace lv
