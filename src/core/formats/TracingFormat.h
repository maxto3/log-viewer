#pragma once

#include "core/LogFormat.h"

namespace lv {

/// Parser for Rust `tracing` / `tracing-subscriber` output, e.g.
/// `2026-09-28T22:15:43.3035871+08:00  INFO tokio-rt-worker ThreadId(27) crate::module: message`
class TracingFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("tracing"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
};

} // namespace lv
