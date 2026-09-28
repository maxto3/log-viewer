#include "core/formats/SyslogFormats.h"

#include "core/LogLevel.h"
#include "core/TimestampParser.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace lv {
namespace {

const QRegularExpression &syslog3164Pattern()
{
    // Mmm dd HH:MM:SS host tag[pid]: message
    static const QRegularExpression re(QStringLiteral(
        R"(^(?<ts>[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2})\s+)"      // timestamp
        R"((?<host>\S+)\s+)"                                             // host
        R"((?<tag>[^\s:\[\]]+)(?:\[(?<pid>\d+)\])?:\s?(?<message>.*)$)")); // tag[pid]: message
    return re;
}

const QRegularExpression &syslog5424Pattern()
{
    static const QRegularExpression re(QStringLiteral(
        R"5424(^<(?<pri>\d{1,3})>(?<version>\d)\s+)5424"                    // <PRI>VERSION
        R"5424((?<ts>\S+)\s+)5424"                                          // TIMESTAMP
        R"5424((?<host>\S+)\s+)5424"                                        // HOSTNAME
        R"5424((?<app>\S+)\s+)5424"                                         // APP-NAME
        R"5424((?<proc>\S+)\s+)5424"                                        // PROCID
        R"5424((?<msgid>\S+)\s*)5424"                                       // MSGID
        R"5424((?:\[(?<sd>[^\]]*)\]\s*)?)5424"                              // optional structured data
        R"5424((?<message>.*)$)5424"));
    return re;
}

} // namespace

int Syslog3164Format::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int hits = 0;
    for (const QString &line : sampleLines) {
        if (syslog3164Pattern().match(line).hasMatch())
            ++hits;
    }
    return (hits * 100) / sampleLines.size();
}

bool Syslog3164Format::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    const QRegularExpressionMatch match = syslog3164Pattern().match(line);
    if (!match.hasMatch())
        return false;

    entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
    entry.host = match.captured(QStringLiteral("host"));
    entry.target = match.captured(QStringLiteral("tag"));
    entry.pid = match.captured(QStringLiteral("pid"));
    entry.message = match.captured(QStringLiteral("message"));
    entry.firstLine = lineNumber;

    // RFC 3164 has no level; syslog daemons often write the severity inside the
    // message ("ERROR: ...").
    if (entry.message.size() > 2 && entry.message.at(0).isUpper()) {
        const int colon = entry.message.indexOf(QLatin1Char(':'));
        if (colon > 0 && colon <= 10) {
            const LogLevel level = logLevelFromString(entry.message.left(colon));
            if (level != LogLevel::Other) {
                entry.level = level;
                entry.rawLevel = entry.message.left(colon);
                entry.message = entry.message.mid(colon + 1).trimmed();
            }
        }
    }
    return true;
}

QString Syslog5424Format::displayName() const
{
    return QCoreApplication::translate("Formats", "syslog RFC 5424");
}

int Syslog5424Format::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int hits = 0;
    for (const QString &line : sampleLines) {
        if (syslog5424Pattern().match(line).hasMatch())
            ++hits;
    }
    return (hits * 100) / sampleLines.size();
}

bool Syslog5424Format::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    const QRegularExpressionMatch match = syslog5424Pattern().match(line);
    if (!match.hasMatch())
        return false;

    const int priority = match.captured(QStringLiteral("pri")).toInt();
    entry.level = logLevelFromSyslogPriority(priority % 8);
    entry.rawLevel = logLevelCanonicalName(entry.level);
    entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
    entry.host = match.captured(QStringLiteral("host"));
    entry.target = match.captured(QStringLiteral("app"));
    entry.pid = match.captured(QStringLiteral("proc"));
    entry.message = match.captured(QStringLiteral("message"));
    entry.firstLine = lineNumber;

    const QString msgId = match.captured(QStringLiteral("msgid"));
    if (!msgId.isEmpty() && msgId != QLatin1String("-"))
        entry.extra.insert(QStringLiteral("MsgId"), msgId);
    const QString structuredData = match.captured(QStringLiteral("sd"));
    if (!structuredData.isEmpty())
        entry.extra.insert(QStringLiteral("SD"), structuredData);
    return true;
}

} // namespace lv
