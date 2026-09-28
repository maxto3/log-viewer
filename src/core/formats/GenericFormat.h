#pragma once

#include "core/LogFormat.h"

namespace lv {

/// Fallback parser: recognises an optional timestamp, an optional level token
/// and treats everything else as the message. Any text file can be viewed.
class GenericFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("generic"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
    /// Only anchor based lines start a new entry; in files without any anchor
    /// (plain text) every line is an entry of its own (REQ-PARSE-06).
    bool startsEntry(const QString &line) const override;

private:
    /// Set while probing: at least one sampled line had a timestamp anchor.
    mutable bool m_anchoredSample = false;
};

} // namespace lv
