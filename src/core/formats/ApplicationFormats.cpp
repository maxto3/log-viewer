#include "core/formats/ApplicationFormats.h"

#include "core/LogLevel.h"
#include "core/TimestampParser.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace lv {
namespace {

constexpr const char *kLevelToken =
    R"((?:TRACE|TRC|VERBOSE|VRB|DEBUG|DBG|INFO|INF|NOTICE|WARN|WARNING|WRN|ERROR|ERR|SEVERE|CRITICAL|CRIT|FATAL|FTL))";

const QRegularExpression &pythonPattern()
{
    static const QRegularExpression re(QStringLiteral(
        R"(^(?<ts>\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2}(?:[.,]\d{1,6})?)\s*[-–]\s*)"   // timestamp
        R"((?<level>)") + QLatin1String(kLevelToken)
        + QStringLiteral(R"()\s*[-–:]\s*(?:(?<logger>[\w.\-]+)\s*[-–:]\s*)?(?<message>.*)$)"));
    return re;
}

const QRegularExpression &serilogDateTimePattern()
{
    static const QRegularExpression re(QStringLiteral(
        R"(^(?<ts>\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2}(?:[.,]\d{1,6})?(?:\s*(?:Z|[+-]\d{2}:?\d{2}))?)\s*)"  // timestamp
        R"(\[(?<level>VRB|DBG|INF|WRN|ERR|FTL)\]\s*(?<message>.*)$)"));
    return re;
}

const QRegularExpression &serilogTimeOnlyPattern()
{
    static const QRegularExpression re(QStringLiteral(
        R"(^\[(?<ts>\d{2}:\d{2}:\d{2}(?:[.,]\d{1,6})?)\s+)"      // [22:15:43
        R"((?<level>VRB|DBG|INF|WRN|ERR|FTL)\]\s*(?<message>.*)$)"));  // INF] message
    return re;
}

const QRegularExpression &log4jPattern()
{
    // 2026-09-28 22:15:43,303 [thread] LEVEL  logger - message
    static const QRegularExpression re(QStringLiteral(
        R"(^(?<ts>\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2}(?:[.,]\d{1,6})?)\s+)"          // timestamp
        R"(\[(?<thread>[^\]]*)\]\s*)"                                                    // [thread]
        R"((?<level>)") + QLatin1String(kLevelToken)
        + QStringLiteral(R"()\s+(?<logger>[\w.$]+)\s*[-:]\s*(?<message>.*)$)"));
    return re;
}

int probeWith(const QRegularExpression &pattern, const QStringList &sampleLines)
{
    if (sampleLines.isEmpty())
        return 0;
    int hits = 0;
    for (const QString &line : sampleLines) {
        if (pattern.match(line).hasMatch())
            ++hits;
    }
    return (hits * 100) / sampleLines.size();
}

} // namespace

QString PythonLoggingFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "Python logging");
}

int PythonLoggingFormat::probe(const QStringList &sampleLines) const
{
    return probeWith(pythonPattern(), sampleLines);
}

bool PythonLoggingFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    const QRegularExpressionMatch match = pythonPattern().match(line);
    if (!match.hasMatch())
        return false;
    entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
    entry.rawLevel = match.captured(QStringLiteral("level"));
    entry.level = logLevelFromString(entry.rawLevel);
    entry.target = match.captured(QStringLiteral("logger"));
    entry.message = match.captured(QStringLiteral("message"));
    entry.firstLine = lineNumber;
    return true;
}

QString SerilogFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "Serilog console");
}

int SerilogFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int hits = 0;
    for (const QString &line : sampleLines) {
        if (serilogDateTimePattern().match(line).hasMatch()
            || serilogTimeOnlyPattern().match(line).hasMatch())
            ++hits;
    }
    return (hits * 100) / sampleLines.size();
}

bool SerilogFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    QRegularExpressionMatch match = serilogDateTimePattern().match(line);
    if (!match.hasMatch())
        match = serilogTimeOnlyPattern().match(line);
    if (!match.hasMatch())
        return false;

    entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
    entry.rawLevel = match.captured(QStringLiteral("level"));
    entry.level = logLevelFromString(entry.rawLevel);
    entry.message = match.captured(QStringLiteral("message"));
    entry.firstLine = lineNumber;
    return true;
}

QString Log4jLogbackFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "log4j / Logback");
}

int Log4jLogbackFormat::probe(const QStringList &sampleLines) const
{
    return probeWith(log4jPattern(), sampleLines);
}

bool Log4jLogbackFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    const QRegularExpressionMatch match = log4jPattern().match(line);
    if (!match.hasMatch())
        return false;
    entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
    entry.thread = match.captured(QStringLiteral("thread"));
    entry.rawLevel = match.captured(QStringLiteral("level"));
    entry.level = logLevelFromString(entry.rawLevel);
    entry.target = match.captured(QStringLiteral("logger"));
    entry.message = match.captured(QStringLiteral("message"));
    entry.firstLine = lineNumber;
    return true;
}

} // namespace lv
