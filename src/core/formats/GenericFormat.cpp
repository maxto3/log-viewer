#include "core/formats/GenericFormat.h"

#include "core/LogLevel.h"
#include "core/TimestampParser.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace lv {
namespace {

constexpr const char *kLevelAlternation =
    R"(TRACE|TRC|VERBOSE|VRB|DEBUG|DBG|INFO|INF|NOTICE|WARN|WARNING|WRN|ERROR|ERR|SEVERE|CRITICAL|CRIT|FATAL|FTL|ALERT|EMERG)";

/// timestamp + level + message
const QRegularExpression &withLevelPattern()
{
    static const QRegularExpression re(QStringLiteral(
        R"(^\s*(?<ts>\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(?:[.,]\d{1,9})?(?:Z|[+-]\d{2}:?\d{2})?)\s+)"  // timestamp
        R"(\[?\(?(?<level>)") + QLatin1String(kLevelAlternation)
        + QStringLiteral(R"()\)?\]?\s*[:\-]?\s+(?<message>.*)$)"));

    return re;
}

/// timestamp + message (no level)
const QRegularExpression &timestampOnlyPattern()
{
    static const QRegularExpression re(QStringLiteral(
        R"(^\s*(?<ts>\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(?:[.,]\d{1,9})?(?:Z|[+-]\d{2}:?\d{2})?)\s+(?<message>.*)$)"));
    return re;
}

/// syslog style: Mmm dd HH:MM:SS host tag: message
const QRegularExpression &syslogLikePattern()
{
    static const QRegularExpression re(QStringLiteral(
        R"(^\s*(?<ts>[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2})\s+(?<message>.*)$)"));
    return re;
}

} // namespace

QString GenericFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "Generic text log");
}

int GenericFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int hits = 0;
    int anchored = 0;
    for (const QString &line : sampleLines) {
        if (withLevelPattern().match(line).hasMatch()
            || timestampOnlyPattern().match(line).hasMatch()
            || syslogLikePattern().match(line).hasMatch()) {
            ++hits;
            ++anchored;
        }
    }
    m_anchoredSample = anchored > 0;
    return (hits * 100) / sampleLines.size();
}

bool GenericFormat::startsEntry(const QString &line) const
{
    if (withLevelPattern().match(line).hasMatch() || timestampOnlyPattern().match(line).hasMatch()
        || syslogLikePattern().match(line).hasMatch()) {
        return true;
    }
    // Without any anchor in the sample the file is treated as plain text: one
    // entry per line. Otherwise a line without an anchor is a continuation line
    // (a stack trace frame or a wrapped message).
    return !m_anchoredSample;
}

bool GenericFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    QRegularExpressionMatch match = withLevelPattern().match(line);
    if (match.hasMatch()) {
        entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
        entry.rawLevel = match.captured(QStringLiteral("level"));
        entry.level = logLevelFromString(entry.rawLevel);
        entry.message = match.captured(QStringLiteral("message"));
        entry.firstLine = lineNumber;
        return true;
    }

    static const QRegularExpression bracketLevel(QStringLiteral(
        R"(^\s*\[?(?<level>)") + QLatin1String(kLevelAlternation)
        + QStringLiteral(R"()\]?\s*[:\-]\s*(?<message>.*)$)"));

    match = timestampOnlyPattern().match(line);
    if (match.hasMatch()) {
        entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
        entry.message = match.captured(QStringLiteral("message"));
        entry.firstLine = lineNumber;
        // A leading level token without a timestamp is still worth extracting.
        const QRegularExpressionMatch levelMatch = bracketLevel.match(entry.message);
        if (levelMatch.hasMatch()) {
            entry.rawLevel = levelMatch.captured(QStringLiteral("level"));
            entry.level = logLevelFromString(entry.rawLevel);
            entry.message = levelMatch.captured(QStringLiteral("message"));
        }
        return true;
    }

    match = syslogLikePattern().match(line);
    if (match.hasMatch()) {
        entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
        entry.message = match.captured(QStringLiteral("message"));
        entry.firstLine = lineNumber;
        return true;
    }

    // Nothing recognisable: keep the whole line as the message.
    entry.message = line;
    entry.firstLine = lineNumber;
    entry.level = LogLevel::Other;
    return true;
}

} // namespace lv
