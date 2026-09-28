#include "core/formats/TracingFormat.h"

#include "core/LogLevel.h"
#include "core/TimestampParser.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace lv {
namespace {

const QRegularExpression &tracingPattern()
{
    // Note: the separator between target and message is ": " (a colon followed
    // by whitespace). Requiring the whitespace is what keeps "a::b::c:" targets
    // intact, because "::" is never followed by a space.
    static const QRegularExpression re(QStringLiteral(
        R"(^(?<ts>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:?\d{2}))\s+)"    // timestamp
        R"((?<level>TRACE|DEBUG|INFO|WARN|ERROR)\s+)"                                             // level
        R"((?:(?<thread>\S+)\s+)?ThreadId\((?<threadId>\d+)\)\s+)"                                 // thread / thread id
        R"((?<target>.*?):\s(?<message>.*)$)"));                                                  // target: message
    return re;
}

} // namespace

QString TracingFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "Rust tracing");
}

int TracingFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int hits = 0;
    for (const QString &line : sampleLines) {
        if (tracingPattern().match(line).hasMatch())
            ++hits;
    }
    return (hits * 100) / sampleLines.size();
}

bool TracingFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    const QRegularExpressionMatch match = tracingPattern().match(line);
    if (!match.hasMatch())
        return false;

    entry.time = parseTimestamp(match.captured(QStringLiteral("ts")));
    entry.rawLevel = match.captured(QStringLiteral("level"));
    entry.level = logLevelFromString(entry.rawLevel);
    entry.thread = match.captured(QStringLiteral("thread"));
    entry.target = match.captured(QStringLiteral("target"));
    entry.message = match.captured(QStringLiteral("message"));
    entry.firstLine = lineNumber;

    const QString threadId = match.captured(QStringLiteral("threadId"));
    if (!threadId.isEmpty())
        entry.extra.insert(QStringLiteral("ThreadId"), threadId);
    if (entry.thread.isEmpty() && !threadId.isEmpty())
        entry.thread = QStringLiteral("ThreadId(%1)").arg(threadId);

    return true;
}

} // namespace lv
