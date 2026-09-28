#include "core/LogLevel.h"

#include <QCoreApplication>
#include <QVector>

namespace lv {
namespace {

struct LevelToken {
    QString token;   ///< upper-cased; QString so non-ASCII names work
    LogLevel level;
};

// Tokens are compared case-insensitively against the trimmed, bracket-stripped
// and upper-cased input, so only upper-case spellings are needed here.
const QVector<LevelToken> &levelTokens()
{
    static const QVector<LevelToken> tokens = {
        {QStringLiteral("TRACE"), LogLevel::Trace},
        {QStringLiteral("TRC"), LogLevel::Trace},
        {QStringLiteral("VERBOSE"), LogLevel::Trace},
        {QStringLiteral("VRB"), LogLevel::Trace},
        {QStringLiteral("DEBUG"), LogLevel::Debug},
        {QStringLiteral("DBG"), LogLevel::Debug},
        {QStringLiteral("D"), LogLevel::Debug},
        {QStringLiteral("INFO"), LogLevel::Info},
        {QStringLiteral("INF"), LogLevel::Info},
        {QStringLiteral("I"), LogLevel::Info},
        {QStringLiteral("INFORMATION"), LogLevel::Info},
        {QStringLiteral("NOTICE"), LogLevel::Notice},
        {QStringLiteral("NOT"), LogLevel::Notice},
        {QStringLiteral("WARN"), LogLevel::Warn},
        {QStringLiteral("WARNING"), LogLevel::Warn},
        {QStringLiteral("WRN"), LogLevel::Warn},
        {QStringLiteral("W"), LogLevel::Warn},
        {QStringLiteral("ERROR"), LogLevel::Error},
        {QStringLiteral("ERR"), LogLevel::Error},
        {QStringLiteral("E"), LogLevel::Error},
        {QStringLiteral("SEVERE"), LogLevel::Error},
        {QStringLiteral("FATAL"), LogLevel::Fatal},
        {QStringLiteral("FTL"), LogLevel::Fatal},
        {QStringLiteral("CRITICAL"), LogLevel::Fatal},
        {QStringLiteral("CRIT"), LogLevel::Fatal},
        {QStringLiteral("ALERT"), LogLevel::Fatal},
        {QStringLiteral("EMERG"), LogLevel::Fatal},
        {QStringLiteral("EMERGENCY"), LogLevel::Fatal},
        {QStringLiteral("PANIC"), LogLevel::Fatal},
        // Windows event log / Event Viewer display names (localised Chinese).
        {QStringLiteral("信息"), LogLevel::Info},
        {QStringLiteral("警告"), LogLevel::Warn},
        {QStringLiteral("错误"), LogLevel::Error},
        {QStringLiteral("严重"), LogLevel::Fatal},
        {QStringLiteral("详细"), LogLevel::Trace},
        {QStringLiteral("成功"), LogLevel::Info},
    };
    return tokens;
}

/// Words that look like a level but are part of normal message text; these must
/// not be treated as a level token.
constexpr const char *kRejectedTokens[] = {"I/O", "E-Mail"};

} // namespace

LogLevel logLevelFromString(QStringView text)
{
    QString token = text.toString().trimmed();

    // Strip decoration often found in log files: "[INFO]", "(warn)", "ERROR:".
    while (!token.isEmpty()) {
        const QChar c = token.front();
        if (c == u'[' || c == u'(' || c == u'{' || c == u'<' || c == u' ' || c == u'\t')
            token.remove(0, 1);
        else
            break;
    }
    while (!token.isEmpty()) {
        const QChar c = token.back();
        if (c == u']' || c == u')' || c == u'}' || c == u'>' || c == u':' || c == u'-'
            || c == u' ' || c == u'\t' || c == u',')
            token.chop(1);
        else
            break;
    }
    if (token.isEmpty() || token.size() > 12)
        return LogLevel::Other;

    const QString upper = token.toUpper();
    for (const char *rejected : kRejectedTokens) {
        if (upper == QLatin1StringView(rejected))
            return LogLevel::Other;
    }
    const QVector<LevelToken> &tokens = levelTokens();
    for (const LevelToken &entry : tokens) {
        if (upper == entry.token)
            return entry.level;
    }
    return LogLevel::Other;
}

LogLevel logLevelFromSyslogPriority(int priority)
{
    switch (priority) {
    case 0: // emergency
    case 1: // alert
    case 2: // critical
        return LogLevel::Fatal;
    case 3: // error
        return LogLevel::Error;
    case 4: // warning
        return LogLevel::Warn;
    case 5: // notice
        return LogLevel::Notice;
    case 6: // informational
        return LogLevel::Info;
    case 7: // debug
        return LogLevel::Debug;
    default:
        return LogLevel::Other;
    }
}

QString logLevelCanonicalName(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace:  return QStringLiteral("TRACE");
    case LogLevel::Debug:  return QStringLiteral("DEBUG");
    case LogLevel::Info:   return QStringLiteral("INFO");
    case LogLevel::Notice: return QStringLiteral("NOTICE");
    case LogLevel::Warn:   return QStringLiteral("WARN");
    case LogLevel::Error:  return QStringLiteral("ERROR");
    case LogLevel::Fatal:  return QStringLiteral("FATAL");
    case LogLevel::Other:  break;
    }
    return QStringLiteral("OTHER");
}

QString logLevelDisplayName(LogLevel level)
{
    const QString canonical = logLevelCanonicalName(level);
    return QCoreApplication::translate("LogLevel", canonical.toUtf8().constData());
}

int logLevelIndex(LogLevel level)
{
    const int index = static_cast<int>(level);
    if (index < 0 || index >= kLogLevelCount)
        return static_cast<int>(LogLevel::Other);
    return index;
}

} // namespace lv
