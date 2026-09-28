#pragma once

#include "core/LogFormat.h"

namespace lv {

/// RFC 3164 syslog: `Mmm dd HH:MM:SS host tag[pid]: message`.
/// Also used for `journalctl` short output (the same shape with a unit name).
class Syslog3164Format : public ILogFormat
{
public:
    explicit Syslog3164Format(QString id, QString displayName)
        : m_id(std::move(id))
        , m_displayName(std::move(displayName))
    {
    }

    QString id() const override { return m_id; }
    QString displayName() const override { return m_displayName; }
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;

private:
    QString m_id;
    QString m_displayName;
};

/// RFC 5424 syslog with structured data:
/// `<PRI>VERSION TIMESTAMP HOST APP PROCID MSGID [SD] MESSAGE`
class Syslog5424Format : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("syslog5424"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
};

} // namespace lv
