#include "core/formats/WindowsFormats.h"

#include "core/LogLevel.h"
#include "core/TimestampParser.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QXmlStreamReader>

namespace lv {
namespace {

const QRegularExpression &fieldLinePattern()
{
    // "TimeCreated : value" (the exported format pads the names to a column).
    static const QRegularExpression re(
        QStringLiteral(R"(^\s*([A-Za-z][A-Za-z0-9_]*)\s*:\s?(.*)$)"));
    return re;
}

const QRegularExpression &timeCreatedPattern()
{
    static const QRegularExpression re(QStringLiteral(R"(^\s*TimeCreated\s*:)"));
    return re;
}

} // namespace

QString WindowsEventTextFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "Windows event text export");
}

int WindowsEventTextFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int blocks = 0;
    int fieldLines = 0;
    for (const QString &line : sampleLines) {
        if (timeCreatedPattern().match(line).hasMatch())
            ++blocks;
        else if (fieldLinePattern().match(line).hasMatch())
            ++fieldLines;
    }
    if (blocks == 0)
        return 0;
    // Heavily penalise plain field lists without any TimeCreated block.
    const int relevant = blocks + fieldLines;
    return qMin(100, (blocks * 100) / qMax(1, relevant) + (blocks > 0 ? 40 : 0));
}

bool WindowsEventTextFormat::startsEntry(const QString &line) const
{
    return timeCreatedPattern().match(line).hasMatch();
}

bool WindowsEventTextFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    const QRegularExpressionMatch match = fieldLinePattern().match(line);
    if (!match.hasMatch())
        return false;
    entry.firstLine = lineNumber;
    entry.message = match.captured(2);
    entry.level = LogLevel::Other;
    return true;
}

bool WindowsEventTextFormat::parseEntry(const QStringList &lines, int firstLineNumber,
                                        bool mergeContinuations, LogEntry &entry) const
{
    Q_UNUSED(mergeContinuations);

    entry = LogEntry();
    entry.physicalLines = lines.size();
    if (lines.isEmpty())
        return false;
    entry.firstLine = firstLineNumber;

    QString currentField;
    QStringList messageParts;
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        const QRegularExpressionMatch match = fieldLinePattern().match(line);
        if (match.hasMatch()) {
            currentField = match.captured(1);
            const QString value = match.captured(2);
            if (currentField.compare(QLatin1String("TimeCreated"), Qt::CaseInsensitive) == 0) {
                entry.time = parseTimestamp(value);
                entry.firstLine = firstLineNumber + i;
            } else if (currentField.compare(QLatin1String("LevelDisplayName"), Qt::CaseInsensitive) == 0
                       || currentField.compare(QLatin1String("Level"), Qt::CaseInsensitive) == 0) {
                entry.rawLevel = value;
                entry.level = logLevelFromString(value);
            } else if (currentField.compare(QLatin1String("ProviderName"), Qt::CaseInsensitive) == 0) {
                entry.target = value;
            } else if (currentField.compare(QLatin1String("Message"), Qt::CaseInsensitive) == 0) {
                if (!value.isEmpty())
                    messageParts << value;
            } else if (currentField.compare(QLatin1String("ProcessId"), Qt::CaseInsensitive) == 0) {
                entry.pid = value;
            } else if (currentField.compare(QLatin1String("MachineName"), Qt::CaseInsensitive) == 0) {
                entry.host = value;
            } else if (!value.isEmpty() && value.size() <= 200) {
                entry.extra.insert(currentField, value);
            }
        } else if (!line.trimmed().isEmpty() && !messageParts.isEmpty()) {
            messageParts << line.trimmed();     // continuation of the Message field
        }
    }
    if (entry.firstLine <= 0)
        entry.firstLine = firstLineNumber;
    entry.message = messageParts.join(QLatin1Char(' '));
    if (entry.message.isEmpty())
        entry.message = lines.join(QLatin1Char('\n'));
    return true;
}

QString IisW3cFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "IIS W3C log");
}

int IisW3cFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int hits = 0;
    QStringList fields;
    for (const QString &line : sampleLines) {
        if (line.startsWith(QLatin1String("#Fields:"), Qt::CaseInsensitive)) {
            hits += 3;
            fields = line.mid(8).trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
            continue;
        }
        if (line.startsWith(QLatin1Char('#')))
            continue;
        if (!fields.isEmpty() && line.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() == fields.size())
            ++hits;
    }
    if (fields.isEmpty() || !fields.contains(QLatin1String("date")))
        return 0;
    m_fields = fields;
    return qMin(100, (hits * 100) / qMax(1, sampleLines.size()));
}

bool IisW3cFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    if (line.startsWith(QLatin1Char('#')) || line.trimmed().isEmpty())
        return false;
    if (m_fields.isEmpty())
        return false;

    const QStringList values = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (values.size() != m_fields.size())
        return false;

    entry.firstLine = lineNumber;
    QString method;
    QString uri;
    QString status;
    for (int i = 0; i < values.size(); ++i) {
        const QString field = m_fields.at(i);
        const QString value = values.at(i);
        if (field == QLatin1String("date")) {
            entry.extra.insert(QStringLiteral("date"), value);
        } else if (field == QLatin1String("time")) {
            const QDate date = QDate::fromString(entry.extra.value(QStringLiteral("date")),
                                                 QStringLiteral("yyyy-MM-dd"));
            entry.time = parseTimestamp(value, date);
            entry.extra.remove(QStringLiteral("date"));
        } else if (field == QLatin1String("cs-method")) {
            method = value;
        } else if (field == QLatin1String("cs-uri-stem")) {
            uri = value;
        } else if (field == QLatin1String("sc-status")) {
            status = value;
        } else if (field == QLatin1String("s-ip")) {
            entry.host = value;
        } else if (field == QLatin1String("cs(User-Agent)")) {
            entry.extra.insert(QStringLiteral("UserAgent"), value);
        } else if (value != QLatin1Char('-')) {
            entry.extra.insert(field, value);
        }
    }

    if (!status.isEmpty()) {
        const int code = status.toInt();
        if (code >= 500)
            entry.level = LogLevel::Error;
        else if (code >= 400)
            entry.level = LogLevel::Warn;
        else
            entry.level = LogLevel::Info;
        entry.rawLevel = status;
    }
    entry.message = QStringLiteral("%1 %2 %3").arg(method, uri, status).trimmed();
    return true;
}

QString WindowsEventTsvFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "Windows event text export (tab separated)");
}

namespace {

/// Level names used by the Event Viewer text export (localised and English).
bool isEventLevelToken(const QString &token)
{
    static const QStringList tokens = {
        QStringLiteral("信息"), QStringLiteral("警告"), QStringLiteral("错误"),
        QStringLiteral("严重"), QStringLiteral("详细"), QStringLiteral("成功"),
        QStringLiteral("Information"), QStringLiteral("Warning"), QStringLiteral("Error"),
        QStringLiteral("Critical"), QStringLiteral("Verbose")};
    const QString trimmed = token.trimmed();
    for (const QString &candidate : tokens) {
        if (trimmed.compare(candidate, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool looksLikeEventTsvHeader(const QString &line)
{
    const QStringList cells = line.split(QLatin1Char('\t'));
    if (cells.size() < 4)
        return false;
    const QString joined = cells.join(QLatin1Char('|'));
    static const QStringList markers = {
        QStringLiteral("级别"), QStringLiteral("Level"), QStringLiteral("日期和时间"),
        QStringLiteral("Date and Time"), QStringLiteral("来源"), QStringLiteral("Source"),
        QStringLiteral("事件 ID"), QStringLiteral("Event ID")};
    int found = 0;
    for (const QString &marker : markers) {
        if (joined.contains(marker, Qt::CaseInsensitive))
            ++found;
    }
    return found >= 3;
}

} // namespace

int WindowsEventTsvFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    if (looksLikeEventTsvHeader(sampleLines.first()))
        return 95;

    int hits = 0;
    for (const QString &line : sampleLines) {
        if (isEventLevelToken(line.section(QLatin1Char('\t'), 0, 0)))
            ++hits;
    }
    return (hits * 100) / sampleLines.size() >= 50 ? 80 : 0;
}

bool WindowsEventTsvFormat::startsEntry(const QString &line) const
{
    if (line.count(QLatin1Char('\t')) < 4)
        return false;
    return isEventLevelToken(line.section(QLatin1Char('\t'), 0, 0));
}

bool WindowsEventTsvFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    const QStringList cells = line.split(QLatin1Char('\t'));
    if (cells.size() < 5 || !isEventLevelToken(cells.at(0)))
        return false;

    entry.firstLine = lineNumber;
    entry.rawLevel = cells.at(0).trimmed();
    entry.level = logLevelFromString(entry.rawLevel);
    entry.time = parseTimestamp(cells.at(1).trimmed());
    entry.target = cells.at(2).trimmed();

    const QString eventId = cells.at(3).trimmed();
    if (!eventId.isEmpty())
        entry.extra.insert(QCoreApplication::translate("Formats", "Event ID"), eventId);

    const QString category = cells.at(4).trimmed();
    if (!category.isEmpty() && category != QLatin1String("-")
        && category.compare(QStringLiteral("无"), Qt::CaseInsensitive) != 0
        && category.compare(QStringLiteral("None"), Qt::CaseInsensitive) != 0) {
        entry.extra.insert(QCoreApplication::translate("Formats", "Task"), category);
    }

    // Everything from the sixth cell on is the message.
    entry.message = cells.mid(5).join(QLatin1Char(' ')).trimmed();
    return true;
}

bool WindowsEventTsvFormat::parseEntry(const QStringList &lines, int firstLineNumber,
                                       bool mergeContinuations, LogEntry &entry) const
{
    if (lines.isEmpty() || !parseLine(lines.first(), firstLineNumber, entry))
        return false;
    entry.physicalLines = lines.size();
    if (!mergeContinuations || lines.size() == 1)
        return true;

    QString message = entry.message;
    // The exporter wraps a multi line message in double quotes; the opening
    // quote sits at the end of the first line, the closing one at the end of the
    // last line.
    if (message.startsWith(QLatin1Char('"')))
        message.remove(0, 1);
    for (int i = 1; i < lines.size(); ++i) {
        QString continuation = lines.at(i);
        message += QLatin1Char('\n') + continuation;
    }
    if (message.endsWith(QLatin1Char('"')))
        message.chop(1);
    entry.message = message;
    return true;
}

QString WindowsEventXmlFormat::displayName() const
{
    return QCoreApplication::translate("Formats", "Windows event XML export");
}

int WindowsEventXmlFormat::probe(const QStringList &sampleLines) const
{
    if (sampleLines.isEmpty())
        return 0;
    int events = 0;
    for (const QString &line : sampleLines)
        events += line.count(QLatin1String("<Event "));
    if (events == 0)
        return 0;
    return qMin(100, 70 + events / 10);
}

bool WindowsEventXmlFormat::parseLine(const QString &line, int lineNumber, LogEntry &entry) const
{
    Q_UNUSED(line);
    Q_UNUSED(lineNumber);
    Q_UNUSED(entry);
    return false;   // the document index parses whole events
}

bool WindowsEventXmlFormat::parseDocument(const QStringList &allLines,
                                          std::vector<LogEntry> &outEntries) const
{
    const QString document = allLines.join(QLatin1Char('\n'));
    const QString openTag = QStringLiteral("<Event ");
    const QString closeTag = QStringLiteral("</Event>");

    int index = 0;
    while (true) {
        const int start = document.indexOf(openTag, index);
        if (start < 0)
            break;
        const int end = document.indexOf(closeTag, start);
        if (end < 0)
            break;

        LogEntry entry;
        if (parseEventXml(document.mid(start, end - start + closeTag.size()), entry)) {
            // The export is usually a single physical line; the source numbers
            // the entries (numbersEntriesSequentially) instead.
            entry.physicalLines = 1;
            outEntries.push_back(entry);
        }
        index = end + closeTag.size();
    }
    return !outEntries.empty();
}

bool WindowsEventXmlFormat::parseEventXml(const QString &xml, LogEntry &entry)
{
    QXmlStreamReader reader(xml);
    QStringList dataValues;
    QString message;
    QString levelName;      ///< localised name from RenderingInfo (preferred)
    int numericLevel = -1;

    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType token = reader.readNext();
        if (token != QXmlStreamReader::StartElement)
            continue;
        const QStringView name = reader.name();
        if (name == QLatin1String("TimeCreated")) {
            entry.time = parseTimestamp(reader.attributes().value(QLatin1String("SystemTime")).toString());
        } else if (name == QLatin1String("Provider")) {
            entry.target = reader.attributes().value(QLatin1String("Name")).toString();
        } else if (name == QLatin1String("EventID")) {
            entry.extra.insert(QCoreApplication::translate("Formats", "Event ID"),
                               reader.readElementText().trimmed());
        } else if (name == QLatin1String("Level")) {
            // <System><Level>2</Level> is numeric, the RenderingInfo one is the
            // localised display name ("信息", "Error", …).
            const QString value = reader.readElementText().trimmed();
            bool isNumber = false;
            const int number = value.toInt(&isNumber);
            if (isNumber) {
                if (numericLevel < 0)
                    numericLevel = number;
            } else if (!value.isEmpty() && levelName.isEmpty()) {
                levelName = value;
            }
        } else if (name == QLatin1String("Computer")) {
            entry.host = reader.readElementText().trimmed();
        } else if (name == QLatin1String("Message")) {
            message = reader.readElementText();
        } else if (name == QLatin1String("Data")) {
            const QString key = reader.attributes().value(QLatin1String("Name")).toString();
            const QString value = reader.readElementText().trimmed();
            if (!value.isEmpty() && value.size() <= 200)
                dataValues << (key.isEmpty() ? value : key + QLatin1Char('=') + value);
        }
    }
    if (reader.hasError())
        return false;

    if (!levelName.isEmpty()) {
        entry.level = logLevelFromString(levelName);
        if (entry.level != LogLevel::Other)
            entry.rawLevel = levelName;
    }
    if (entry.level == LogLevel::Other && numericLevel >= 0) {
        // Event log levels: 0 log always (informational), 1 critical … 5 verbose.
        switch (numericLevel) {
        case 0: entry.level = LogLevel::Info; break;
        case 1: entry.level = LogLevel::Fatal; break;
        case 2: entry.level = LogLevel::Error; break;
        case 3: entry.level = LogLevel::Warn; break;
        case 4: entry.level = LogLevel::Info; break;
        case 5: entry.level = LogLevel::Trace; break;
        default: break;
        }
        if (entry.level != LogLevel::Other)
            entry.rawLevel = logLevelCanonicalName(entry.level);
    }

    entry.message = message.trimmed();
    if (entry.message.isEmpty())
        entry.message = dataValues.join(QStringLiteral("; "));
    if (entry.message.isEmpty())
        entry.message = xml.left(500);
    if (!dataValues.isEmpty())
        entry.extra.insert(QStringLiteral("Data"), dataValues.join(QStringLiteral("; ")).left(200));
    return true;
}

bool WindowsEventXmlFormat::parseEntry(const QStringList &lines, int firstLineNumber,
                                       bool mergeContinuations, LogEntry &entry) const
{
    Q_UNUSED(mergeContinuations);
    if (!parseEventXml(lines.join(QLatin1Char('\n')), entry))
        return false;
    entry.firstLine = firstLineNumber;
    entry.physicalLines = lines.size();
    return true;
}

} // namespace lv
