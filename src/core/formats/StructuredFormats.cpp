#include "core/formats/StructuredFormats.h"

#include "core/LogLevel.h"
#include "core/TimestampParser.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

namespace lv {
namespace {

QString jsonValueToString(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 16);
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isNull() || value.isUndefined())
        return {};
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
}

/// Looks up the first present key of \a candidates (case-insensitive fallback).
QString valueForKeys(const QJsonObject &object, const QStringList &keys, bool *found = nullptr)
{
    for (const QString &key : keys) {
        const auto it = object.constFind(key);
        if (it != object.constEnd()) {
            if (found)
                *found = true;
            return jsonValueToString(*it);
        }
        // Case-insensitive fallback (e.g. "Time" vs "time").
        for (auto objIt = object.constBegin(); objIt != object.constEnd(); ++objIt) {
            if (objIt.key().compare(key, Qt::CaseInsensitive) == 0) {
                if (found)
                    *found = true;
                return jsonValueToString(*objIt);
            }
        }
    }
    return {};
}

QDateTime timestampFromValue(const QString &value)
{
    if (value.isEmpty())
        return {};
    bool isNumber = false;
    const double numeric = value.toDouble(&isNumber);
    if (isNumber && numeric > 0) {
        // Epoch seconds / milliseconds / microseconds.
        qint64 millis = 0;
        if (numeric > 1e15)
            millis = static_cast<qint64>(numeric / 1000.0);      // microseconds
        else if (numeric > 1e12)
            millis = static_cast<qint64>(numeric);               // milliseconds
        else
            millis = static_cast<qint64>(numeric * 1000.0);      // seconds
        return QDateTime::fromMSecsSinceEpoch(millis);
    }
    return parseTimestamp(value);
}

const QStringList &timeKeys()
{
    static const QStringList keys = {QStringLiteral("@timestamp"), QStringLiteral("timestamp"),
                                     QStringLiteral("time"), QStringLiteral("ts"),
                                     QStringLiteral("datetime"), QStringLiteral("TimeCreated"),
                                     QStringLiteral("__REALTIME_TIMESTAMP"),
                                     QStringLiteral("_SOURCE_REALTIME_TIMESTAMP")};
    return keys;
}

const QStringList &levelKeys()
{
    static const QStringList keys = {QStringLiteral("level"), QStringLiteral("severity"),
                                     QStringLiteral("lvl"), QStringLiteral("log_level"),
                                     QStringLiteral("levelname"), QStringLiteral("LevelDisplayName"),
                                     QStringLiteral("PRIORITY")};
    return keys;
}

const QStringList &messageKeys()
{
    static const QStringList keys = {QStringLiteral("message"), QStringLiteral("msg"),
                                     QStringLiteral("MESSAGE"), QStringLiteral("text"),
                                     QStringLiteral("event"), QStringLiteral("renderedMessage")};
    return keys;
}

const QStringList &hostKeys()
{
    static const QStringList keys = {QStringLiteral("host"), QStringLiteral("hostname"),
                                     QStringLiteral("_HOSTNAME")};
    return keys;
}

const QStringList &pidKeys()
{
    static const QStringList keys = {QStringLiteral("pid"), QStringLiteral("procid"),
                                     QStringLiteral("process"), QStringLiteral("_PID"),
                                     QStringLiteral("ProcessId")};
    return keys;
}

const QStringList &targetKeys()
{
    static const QStringList keys = {QStringLiteral("logger"), QStringLiteral("target"),
                                     QStringLiteral("module"), QStringLiteral("source"),
                                     QStringLiteral("category"), QStringLiteral("SYSLOG_IDENTIFIER"),
                                     QStringLiteral("ProviderName"), QStringLiteral("name")};
    return keys;
}

const QStringList &timestampFormats()
{
    static const QStringList formats = {QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"),
                                        QStringLiteral("yyyy-MM-dd HH:mm:ss"),
                                        QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz")};
    return formats;
}

} // namespace

int JsonLinesFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int hits = 0;
    for (const QString &line : sampleLines) {
        const QByteArray trimmed = line.trimmed().toUtf8();
        if (!trimmed.startsWith('{'))
            continue;
        const QJsonDocument document = QJsonDocument::fromJson(trimmed);
        if (!document.isObject())
            continue;
        const QJsonObject object = document.object();
        if (m_journalMode) {
            if (!object.contains(QStringLiteral("MESSAGE"))
                || !object.contains(QStringLiteral("PRIORITY")))
                continue;
        } else if (object.contains(QStringLiteral("__REALTIME_TIMESTAMP"))) {
            continue;    // journald output belongs to the journal format
        }
        ++hits;
    }
    return (hits * 100) / sampleLines.size();
}

bool JsonLinesFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    const QByteArray trimmed = line.trimmed().toUtf8();
    if (!trimmed.startsWith('{'))
        return false;
    const QJsonDocument document = QJsonDocument::fromJson(trimmed);
    if (!document.isObject())
        return false;
    const QJsonObject object = document.object();

    entry.firstLine = lineNumber;
    entry.time = timestampFromValue(valueForKeys(object, timeKeys()));
    entry.rawLevel = valueForKeys(object, levelKeys());
    if (!entry.rawLevel.isEmpty()) {
        bool numeric = false;
        const int priority = entry.rawLevel.toInt(&numeric);
        entry.level = numeric ? logLevelFromSyslogPriority(priority)
                              : logLevelFromString(entry.rawLevel);
    }
    entry.message = valueForKeys(object, messageKeys());
    entry.host = valueForKeys(object, hostKeys());
    entry.pid = valueForKeys(object, pidKeys());
    entry.target = valueForKeys(object, targetKeys());

    // Remaining keys become extra columns.
    static const QStringList knownKeys = [] {
        QStringList keys;
        keys << timeKeys() << levelKeys() << messageKeys() << hostKeys() << pidKeys()
             << targetKeys();
        return keys;
    }();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool known = false;
        for (const QString &key : knownKeys) {
            if (it.key().compare(key, Qt::CaseInsensitive) == 0) {
                known = true;
                break;
            }
        }
        if (known || it.key().startsWith(QLatin1Char('_')))
            continue;
        const QString value = jsonValueToString(*it);
        if (value.size() <= 200)
            entry.extra.insert(it.key(), value);
    }
    return true;
}

QString LogfmtFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "logfmt (key=value)");
}

int LogfmtFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    static const QRegularExpression pair(QStringLiteral(R"lfx((^|\s)[A-Za-z_][\w.\-]*=)lfx"));
    int hits = 0;
    for (const QString &line : sampleLines) {
        const int pairs = line.count(pair);
        if (pairs >= 2 && (line.contains(QLatin1String("level="))
                           || line.contains(QLatin1String("msg="))
                           || line.contains(QLatin1String("time="))
                           || line.contains(QLatin1String("ts="))))
            ++hits;
    }
    return (hits * 100) / sampleLines.size();
}

bool LogfmtFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    static const QRegularExpression pairPattern(
        QStringLiteral(R"lfx(([A-Za-z_][\w.\-]*)=("([^"]*)"|\S*))lfx"));

    QHash<QString, QString> values;
    QStringList plainWords;
    int consumed = 0;
    QRegularExpressionMatchIterator it = pairPattern.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        values.insert(match.captured(1), match.captured(3).isEmpty() ? match.captured(2)
                                                                    : match.captured(3));
        consumed += static_cast<int>(match.capturedLength());
    }

    // A logfmt line without a single pair is not logfmt.
    if (values.isEmpty())
        return false;

    entry.firstLine = lineNumber;
    const auto take = [&values](const QStringList &keys) {
        for (const QString &key : keys) {
            const auto found = values.constFind(key);
            if (found != values.constEnd()) {
                const QString value = *found;
                values.erase(found);
                return value;
            }
        }
        return QString();
    };

    entry.time = timestampFromValue(take(timeKeys()));
    entry.rawLevel = take(levelKeys());
    if (!entry.rawLevel.isEmpty())
        entry.level = logLevelFromString(entry.rawLevel);
    entry.message = take(messageKeys());
    entry.host = take(hostKeys());
    entry.pid = take(pidKeys());
    entry.target = take(targetKeys());

    const QString trimmed = line.trimmed();
    if (consumed * 100 < trimmed.size() * 50) {
        // More than half of the line is not key=value: keep it as the message
        // when the structured message field was absent.
        const QString remainder = trimmed.mid(consumed).trimmed();
        if (entry.message.isEmpty() && !remainder.isEmpty())
            entry.message = remainder;
        else if (!remainder.isEmpty())
            entry.message += QLatin1Char(' ') + remainder;
    }
    if (entry.message.isEmpty())
        entry.message = trimmed;

    for (auto it2 = values.constBegin(); it2 != values.constEnd(); ++it2)
        entry.extra.insert(it2.key(), it2.value());
    Q_UNUSED(plainWords);
    return true;
}

QString CsvFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "CSV / TSV table");
}

QStringList CsvFormat::splitRow(const QString &line, QChar delimiter)
{
    QStringList cells;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    current.append(QLatin1Char('"'));
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current.append(c);
            }
        } else if (c == QLatin1Char('"')) {
            inQuotes = true;
        } else if (c == delimiter) {
            cells << current;
            current.clear();
        } else {
            current.append(c);
        }
    }
    cells << current;
    return cells;
}

int CsvFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.size() < 2)
        return 0;
    static const QList<QChar> candidates = {QLatin1Char(','), QLatin1Char(';'),
                                            QLatin1Char('\t'), QLatin1Char('|')};

    const QString header = sampleLines.first();
    QChar best;
    int bestCount = 0;
    for (QChar candidate : candidates) {
        const int count = header.count(candidate);
        if (count > bestCount) {
            bestCount = count;
            best = candidate;
        }
    }
    if (bestCount == 0)
        return 0;

    // Header names must look like column titles, not like a data row.
    const QStringList headerCells = splitRow(header, best);
    int named = 0;
    static const QRegularExpression namePattern(QStringLiteral(R"(^[A-Za-z_][\w .\-]*$)"));
    for (const QString &cell : headerCells) {
        if (namePattern.match(cell.trimmed()).hasMatch())
            ++named;
    }
    if (named * 100 < headerCells.size() * 70)
        return 0;

    // Data rows should have a similar cell count.
    int consistent = 0;
    const int limit = qMin(sampleLines.size(), 20);
    for (int i = 1; i < limit; ++i) {
        if (splitRow(sampleLines.at(i), best).size() == headerCells.size())
            ++consistent;
    }
    if (consistent * 100 < (limit - 1) * 60)
        return 0;

    m_header = headerCells;
    m_delimiter = best;
    m_headerLine = header;
    return 90;
}

bool CsvFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    if (m_header.isEmpty() || m_delimiter.isNull())
        return false;    // no header captured yet (probe has to run first)
    if (line.trimmed().isEmpty() || line == m_headerLine)
        return false;    // the header row itself is not an entry

    const QStringList cells = splitRow(line, m_delimiter);
    entry.firstLine = lineNumber;
    for (int i = 0; i < cells.size() && i < m_header.size(); ++i) {
        const QString name = m_header.at(i).trimmed();
        const QString value = cells.at(i).trimmed();
        if (name.isEmpty())
            continue;
        const QString lower = name.toLower();

        if (lower.contains(QLatin1String("time")) || lower == QLatin1String("date")) {
            if (lower == QLatin1String("date") && i + 1 < cells.size()) {
                entry.time = timestampFromValue(value + QLatin1Char(' ') + cells.at(i + 1).trimmed());
            } else if (!entry.time.isValid()) {
                entry.time = timestampFromValue(value);
            }
        } else if (lower.contains(QLatin1String("level")) || lower.contains(QLatin1String("severity"))) {
            entry.rawLevel = value;
            entry.level = logLevelFromString(value);
        } else if (lower == QLatin1String("message") || lower == QLatin1String("msg")
                   || lower == QLatin1String("eventdata")) {
            if (entry.message.isEmpty())
                entry.message = value;
            else
                entry.extra.insert(name, value);
        } else if (lower == QLatin1String("host") || lower == QLatin1String("hostname")) {
            entry.host = value;
        } else if (lower == QLatin1String("pid") || lower == QLatin1String("processid")) {
            entry.pid = value;
        } else if (lower == QLatin1String("logger") || lower == QLatin1String("target")
                   || lower == QLatin1String("source") || lower == QLatin1String("providername")) {
            entry.target = value;
        } else if (value.size() <= 200) {
            entry.extra.insert(name, value);
        }
    }
    if (entry.message.isEmpty()) {
        // No message column: join the remaining cells so nothing is lost.
        QStringList rest;
        for (int i = 0; i < cells.size() && i < m_header.size(); ++i)
            rest << m_header.at(i).trimmed() + QLatin1Char('=') + cells.at(i).trimmed();
        entry.message = rest.join(QLatin1Char(' '));
    }
    return true;
}

} // namespace lv
