#pragma once

#include <QFont>
#include <QString>

/// Platform specific facts and helpers.
///
/// This is one of the very few places allowed to contain platform conditionals
/// (see spec.md CON-9 / REQ-PLAT-01); everything platform related must be
/// centralised here instead of being spread over the code base.
namespace lv::PlatformInfo {

/// Human readable operating system name, e.g. "Windows 11" or "Debian 13".
QString osName();

/// Directory holding settings.ini (created on first use).
/// Windows: %APPDATA%\LogViewer  |  Linux: $XDG_CONFIG_HOME/LogViewer
QString configDirPath();

/// Full path of the settings file.
QString settingsFilePath();

/// Preferred monospace family for log content (Consolas on Windows, the
/// system fixed font elsewhere).
QString monospaceFontFamily();

/// QFont for the given point size, based on monospaceFontFamily().
QFont monospaceFont(int pointSize = 10);

/// True when the system reports a dark colour scheme.
bool systemThemeIsDark();

/// Compiler, Qt version and build timestamp, shown in the About dialog.
QString buildInfo();

} // namespace lv::PlatformInfo
