#include "core/AnsiText.h"
#include "highlight/AnsiPalette.h"

#include <QTest>

#include <algorithm>
#include <cmath>

using namespace lv;

namespace {

double relativeLuminance(const QColor &color)
{
    const auto channel = [](double value) {
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF())
        + 0.0722 * channel(color.blueF());
}

double contrastRatio(const QColor &a, const QColor &b)
{
    const double first = relativeLuminance(a);
    const double second = relativeLuminance(b);
    return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
}

} // namespace

/// ANSI escape handling (spec.md REQ-PARSE-12): control sequences are removed,
/// SGR attributes are reported as spans over the cleaned text.
class TestAnsiText : public QObject
{
    Q_OBJECT

private slots:
    void plainTextIsUnchangedAndHasNoSpans();
    void sgrColoursBecomeSpans();
    void resetSplitsSpans();
    void cursorAndOscSequencesAreStripped();
    void controlCharactersAreRemoved();
    void tabsAndNewlinesAreKept();
    void boldUnderlineAndExtendedColours();
    void trueColourAnd256Greys();
    void unterminatedSequenceIsStripped();
    void twoCharacterEscapeIsStripped();
    void spanOffsetsSurviveStripping();
    void paletteColoursMeetContrastInBothThemes();
};

void TestAnsiText::plainTextIsUnchangedAndHasNoSpans()
{
    const QString plain = QStringLiteral("2026-10-01 01:15:15 INFO boot one");
    QCOMPARE(AnsiText::strip(plain), plain);
    const AnsiTextResult result = AnsiText::process(plain);
    QCOMPARE(result.text, plain);
    QVERIFY(result.spans.isEmpty());
    QVERIFY(!result.hadEscapes);
}

void TestAnsiText::sgrColoursBecomeSpans()
{
    const AnsiTextResult result =
        AnsiText::process(QStringLiteral("\x1b[31mred\x1b[0m plain"));
    QCOMPARE(result.text, QStringLiteral("red plain"));
    QCOMPARE(result.spans.size(), 1);
    QCOMPARE(result.spans.at(0).start, 0);
    QCOMPARE(result.spans.at(0).length, 3);
    QCOMPARE(result.spans.at(0).style.foreground, 1);
    QVERIFY(!result.spans.at(0).style.hasRgbForeground);
    QVERIFY(result.hadEscapes);
}

void TestAnsiText::resetSplitsSpans()
{
    const AnsiTextResult result =
        AnsiText::process(QStringLiteral("\x1b[31mred\x1b[0mplain\x1b[1mbold"));
    QCOMPARE(result.text, QStringLiteral("redplainbold"));
    QCOMPARE(result.spans.size(), 2);
    QCOMPARE(result.spans.at(0).start, 0);
    QCOMPARE(result.spans.at(0).length, 3);
    QCOMPARE(result.spans.at(0).style.foreground, 1);
    QCOMPARE(result.spans.at(1).start, 8);
    QCOMPARE(result.spans.at(1).length, 4);
    QVERIFY(result.spans.at(1).style.bold);
    QCOMPARE(result.spans.at(1).style.foreground, -1);
}

void TestAnsiText::cursorAndOscSequencesAreStripped()
{
    // Cursor queries, window operations, charset selection and OSC titles are
    // noise: they disappear completely (REQ-PARSE-12).
    const QString input = QStringLiteral("A\x1b[6nB\x1b[?7hC\x1b]0;window title")
        + QChar(0x07) + QStringLiteral("D\x1b[18tE\x1b( BF");
    const AnsiTextResult result = AnsiText::process(input);
    QCOMPARE(result.text, QStringLiteral("ABCDEF"));
    QVERIFY(result.spans.isEmpty());
}

void TestAnsiText::controlCharactersAreRemoved()
{
    const QString input = QStringLiteral("a\rb%1c%2d%3e")
                              .arg(QChar(0x07))
                              .arg(QChar(0x7F))
                              .arg(QChar(0x85));
    const AnsiTextResult result = AnsiText::process(input);
    QCOMPARE(result.text, QStringLiteral("abcde"));
}

void TestAnsiText::twoCharacterEscapeIsStripped()
{
    // ESC followed by a final byte is a complete escape sequence (e.g. ESC d =
    // vertical position absolute) and disappears with it.
    const QString input = QStringLiteral("x\x1b") + QStringLiteral("dy");
    QCOMPARE(AnsiText::strip(input), QStringLiteral("xy"));
}

void TestAnsiText::tabsAndNewlinesAreKept()
{
    const QString input = QStringLiteral("a\tb\nc");
    QCOMPARE(AnsiText::strip(input), input);
}

void TestAnsiText::boldUnderlineAndExtendedColours()
{
    const AnsiTextResult result = AnsiText::process(
        QStringLiteral("\x1b[1;4;38;5;196mX\x1b[0m"));
    QCOMPARE(result.text, QStringLiteral("X"));
    QCOMPARE(result.spans.size(), 1);
    const AnsiStyle &style = result.spans.at(0).style;
    QVERIFY(style.bold);
    QVERIFY(style.underline);
    QVERIFY(!style.italic);
    QVERIFY(style.hasRgbForeground);
    QCOMPARE(style.rgbForeground, QColor(255, 0, 0));
    QCOMPARE(style.foreground, -1);
}

void TestAnsiText::trueColourAnd256Greys()
{
    const AnsiTextResult trueColour = AnsiText::process(
        QStringLiteral("\x1b[38;2;10;20;30mT\x1b[0m"));
    QVERIFY(trueColour.spans.at(0).style.hasRgbForeground);
    QCOMPARE(trueColour.spans.at(0).style.rgbForeground, QColor(10, 20, 30));

    const AnsiTextResult gray = AnsiText::process(
        QStringLiteral("\x1b[48;5;240mG\x1b[0m"));
    QVERIFY(gray.spans.at(0).style.hasRgbBackground);
    QCOMPARE(gray.spans.at(0).style.rgbBackground, QColor(88, 88, 88));
}

void TestAnsiText::unterminatedSequenceIsStripped()
{
    QCOMPARE(AnsiText::strip(QStringLiteral("abc\x1b[")), QStringLiteral("abc"));
    QCOMPARE(AnsiText::strip(QStringLiteral("abc\x1b")), QStringLiteral("abc"));
}

void TestAnsiText::spanOffsetsSurviveStripping()
{
    const AnsiTextResult result =
        AnsiText::process(QStringLiteral("pre \x1b[32mgreen\x1b[0m post"));
    QCOMPARE(result.text, QStringLiteral("pre green post"));
    QCOMPARE(result.spans.size(), 1);
    QCOMPARE(result.spans.at(0).start, 4);
    QCOMPARE(result.spans.at(0).length, 5);
    QCOMPARE(result.spans.at(0).style.foreground, 2);
}

void TestAnsiText::paletteColoursMeetContrastInBothThemes()
{
    // REQ-PARSE-12 / REQ-VIS-10: every terminal colour stays readable on the
    // table background of its theme.
    const QColor lightBackground(0xFF, 0xFF, 0xFF);
    const QColor darkBackground(0x1E, 0x1E, 0x1E);
    for (int index = 0; index < 16; ++index) {
        const QColor light = AnsiPalette::color(index, false);
        QVERIFY2(contrastRatio(light, lightBackground) >= 4.5,
                 qPrintable(QStringLiteral("light %1: %2").arg(index).arg(light.name())));
        const QColor dark = AnsiPalette::color(index, true);
        QVERIFY2(contrastRatio(dark, darkBackground) >= 4.5,
                 qPrintable(QStringLiteral("dark %1: %2").arg(index).arg(dark.name())));
    }
}

QTEST_MAIN(TestAnsiText)
#include "tst_ansitext.moc"
