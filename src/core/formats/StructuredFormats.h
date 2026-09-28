#pragma once

#include "core/LogFormat.h"

#include <QStringList>

namespace lv {

/// One JSON object per line (serilog compact, structured loggers).
/// With \a journalMode the journald key set (__REALTIME_TIMESTAMP, PRIORITY,
/// MESSAGE, _HOSTNAME, …) is used; otherwise the generic key set is mapped.
class JsonLinesFormat : public ILogFormat
{
public:
    JsonLinesFormat(QString id, QString displayName, bool journalMode)
        : m_id(std::move(id))
        , m_displayName(std::move(displayName))
        , m_journalMode(journalMode)
    {
    }

    QString id() const override { return m_id; }
    QString displayName() const override { return m_displayName; }
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;

private:
    QString m_id;
    QString m_displayName;
    bool m_journalMode = false;
};

/// `key=value key2="quoted value"` (logfmt / Go structured loggers).
class LogfmtFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("logfmt"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
};

/// CSV / TSV / semicolon separated values with a header row.
///
/// The header is captured while probing the file head; column names are mapped
/// case-insensitively onto the standard fields and unknown columns are kept as
/// extra columns.
class CsvFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("csv_tsv"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;

    static QStringList splitRow(const QString &line, QChar delimiter);

private:
    mutable QStringList m_header;
    mutable QString m_headerLine;
    mutable QChar m_delimiter;
};

} // namespace lv
