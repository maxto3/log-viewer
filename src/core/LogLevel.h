#pragma once

#include <QString>

namespace lv {

/// Canonical log levels used across the application.
/// "Other" is the fallback for unrecognised or missing level information.
enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Notice,
    Warn,
    Error,
    Fatal,
    Other
};

inline constexpr int kLogLevelCount = 8;

/// Maps a raw level token (e.g. "INFO", "wrn", "[ERROR]", "FTL") to a LogLevel.
LogLevel logLevelFromString(QStringView text);

/// Maps a syslog numeric severity (0 = emergency .. 7 = debug) to a LogLevel.
LogLevel logLevelFromSyslogPriority(int priority);

/// Uppercase canonical name, not translated (used for keys and column values).
QString logLevelCanonicalName(LogLevel level);

/// Translated display name; falls back to the canonical name for "Other".
QString logLevelDisplayName(LogLevel level);

/// Stable index in [0, kLogLevelCount) matching the enum order.
int logLevelIndex(LogLevel level);

} // namespace lv
