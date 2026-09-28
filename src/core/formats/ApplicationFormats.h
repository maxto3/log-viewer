#pragma once

#include "core/LogFormat.h"

namespace lv {

/// Python `logging` default format:
/// `2026-09-28 22:15:43,303 - LEVEL - logger - message`
class PythonLoggingFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("python_logging"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
};

/// Serilog console output: `2026-09-28 22:15:43.303 +08:00 [INF] message`
/// and the time-only variant `[22:15:43 INF] message`.
class SerilogFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("serilog"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
};

/// Log4j / Logback pattern:
/// `2026-09-28 22:15:43,303 [thread] LEVEL logger - message`
class Log4jLogbackFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("log4j_logback"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
};

} // namespace lv
