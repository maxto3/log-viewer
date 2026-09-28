#include "core/LogFormat.h"

namespace lv {

bool ILogFormat::startsEntry(const QString &line) const
{
    LogEntry scratch;
    return parseLine(line, 1, scratch);
}

bool ILogFormat::parseEntry(const QStringList &lines, int firstLineNumber,
                            bool mergeContinuations, LogEntry &entry) const
{
    if (lines.isEmpty())
        return false;

    if (!parseLine(lines.first(), firstLineNumber, entry)) {
        entry = LogEntry();
        entry.firstLine = firstLineNumber;
        entry.message = lines.join(QLatin1Char('\n'));
        entry.level = LogLevel::Other;
        return true;
    }

    entry.physicalLines = 1;
    if (mergeContinuations && lines.size() > 1) {
        QStringList parts;
        parts.reserve(lines.size());
        parts << entry.message;
        for (int i = 1; i < lines.size(); ++i)
            parts << lines.at(i);
        entry.message = parts.join(QLatin1Char('\n'));
        entry.physicalLines = lines.size();
    }
    return true;
}

} // namespace lv
