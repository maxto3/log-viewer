#include "app/ThemeManager.h"

#include "platform/PlatformInfo.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

namespace lv {
namespace {

QPalette darkPalette()
{
    QPalette palette;
    const QColor window(0x25, 0x25, 0x26);
    const QColor base(0x1E, 0x1E, 0x1E);
    const QColor text(0xD4, 0xD4, 0xD4);

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, QColor(0x2A, 0x2A, 0x2B));
    palette.setColor(QPalette::ToolTipBase, window);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, QColor(0x33, 0x33, 0x37));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, QColor(0xFF, 0x6B, 0x6B));
    palette.setColor(QPalette::Link, QColor(0x4E, 0xC9, 0xB0));
    palette.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    palette.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    palette.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));

    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6A, 0x6A, 0x6A));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6A, 0x6A, 0x6A));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6A, 0x6A, 0x6A));
    return palette;
}

QPalette lightPalette()
{
    QPalette palette = QApplication::style()->standardPalette();
    palette.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
    palette.setColor(QPalette::AlternateBase, QColor(0xF7, 0xF7, 0xF8));
    palette.setColor(QPalette::Window, QColor(0xF3, 0xF3, 0xF3));
    palette.setColor(QPalette::Text, QColor(0x1F, 0x1F, 0x1F));
    palette.setColor(QPalette::WindowText, QColor(0x1F, 0x1F, 0x1F));
    palette.setColor(QPalette::Highlight, QColor(0x0A, 0x5F, 0xBF));
    palette.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    return palette;
}

QString styleSheet(bool dark)
{
    const QString border = dark ? QStringLiteral("#3F3F46") : QStringLiteral("#C8C8C8");
    // Persistent status bar warning (REQ-REL-01); both colours keep at least
    // 4.5:1 contrast on the corresponding status bar background (REQ-VIS-10).
    const QString warning = dark ? QStringLiteral("#FF6B6B") : QStringLiteral("#C62828");
    // Prominent "Open as Administrator…" action (REQ-REL-04): filled warning
    // colour so it stands out in the status bar, with a contrasting label.
    const QString elevateText = dark ? QStringLiteral("#1F1F1F") : QStringLiteral("#FFFFFF");
    const QString elevateBorder = dark ? QStringLiteral("#C94F4F") : QStringLiteral("#8E1A1A");
    const QString elevateHover = dark ? QStringLiteral("#FF8585") : QStringLiteral("#B21F1F");
    const QString elevatePressed = dark ? QStringLiteral("#E85D5D") : QStringLiteral("#9C1B1B");

    const QString common = QStringLiteral(
                               "QGroupBox {"
                               "  border: 1px solid %1;"
                               "  border-radius: 4px;"
                               "  margin-top: 12px;"
                               "  padding: 12px 10px 10px 10px;"
                               "}"
                               "QGroupBox::title {"
                               "  subcontrol-origin: margin;"
                               "  subcontrol-position: top left;"
                               "  left: 10px;"
                               "  padding: 0 4px;"
                               "  font-weight: 600;"
                               "}"
                               "QStatusBar::item { border: none; }"
                               "QLabel#statusWarning {"
                               "  color: %2;"
                               "  font-weight: 600;"
                               "}"
                               "QLabel#emptyHint[warning=\"true\"] {"
                               "  color: %2;"
                               "}")
                               .arg(border, warning);
    const QString elevate = QStringLiteral(
                                "QPushButton#elevateButton {"
                                "  background-color: %1;"
                                "  color: %2;"
                                "  font-weight: 600;"
                                "  border: 1px solid %3;"
                                "  border-radius: 3px;"
                                "  padding: 0px 10px;"
                                "}"
                                "QPushButton#elevateButton:hover {"
                                "  background-color: %4;"
                                "}"
                                "QPushButton#elevateButton:pressed {"
                                "  background-color: %5;"
                                "}")
                                .arg(warning)
                                .arg(elevateText)
                                .arg(elevateBorder)
                                .arg(elevateHover)
                                .arg(elevatePressed);
    return common + elevate;
}

} // namespace

ThemeManager::ThemeManager(SettingsStore *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
            if (m_settings->theme() == SettingsStore::Theme::System)
                apply();
        });
    }
}

void ThemeManager::apply()
{
    const SettingsStore::Theme mode = m_settings->theme();
    const bool dark = mode == SettingsStore::Theme::Dark
        || (mode == SettingsStore::Theme::System && PlatformInfo::systemThemeIsDark());

    applyPalette(dark);
    applyStyleSheet(dark);

    const bool changed = !m_appliedOnce || dark != m_dark;
    m_dark = dark;
    m_appliedOnce = true;
    if (changed)
        emit themeChanged(m_dark);
}

void ThemeManager::applyPalette(bool dark)
{
    QApplication::setPalette(dark ? darkPalette() : lightPalette());
}

void ThemeManager::applyStyleSheet(bool dark)
{
    qApp->setStyleSheet(styleSheet(dark));
}

} // namespace lv
