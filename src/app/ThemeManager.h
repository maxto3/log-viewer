#pragma once

#include "app/SettingsStore.h"

#include <QObject>

namespace lv {

/// Applies the light/dark/system palette and the small amount of application
/// styling that cannot be expressed with a palette alone.
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(SettingsStore *settings, QObject *parent = nullptr);

    /// Re-reads the setting and applies it.
    void apply();

    bool isDark() const { return m_dark; }

signals:
    void themeChanged(bool dark);

private:
    void applyPalette(bool dark);
    void applyStyleSheet(bool dark);

    SettingsStore *m_settings = nullptr;
    bool m_dark = false;
    bool m_appliedOnce = false;
};

} // namespace lv
