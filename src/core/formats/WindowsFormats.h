#pragma once

#include "core/LogFormat.h"

#include <vector>

namespace lv {

/// Windows event log exported as text (`Get-WinEvent | Format-List`):
///
///     TimeCreated : 2026-09-28T22:15:43.3035871+08:00
///     ProviderName : Microsoft-Windows-Kernel-Power
///     EventId : 42
///     LevelDisplayName : Error
///     Message : the message, possibly with continuation lines
///
/// A block format: one entry consists of several physical lines.
class WindowsEventTextFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("wevt_text"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
    bool startsEntry(const QString &line) const override;
    bool parseEntry(const QStringList &lines, int firstLineNumber, bool mergeContinuations,
                    LogEntry &entry) const override;
};

/// IIS W3C extended log: `#Fields: date time s-ip cs-method cs-uri-stem sc-status …`
/// followed by space separated data rows.
class IisW3cFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("iis_w3c"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;

private:
    mutable QStringList m_fields;
};

/// Event Viewer "Save all events as text" export: tab separated rows
///
///     Level<TAB>Date and Time<TAB>Source<TAB>Event ID<TAB>Task Category<TAB>Message
///
/// The message may span several lines and is then quoted; those lines are
/// continuation lines (REQ-PARSE-06).
class WindowsEventTsvFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("wevt_tsv"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
    bool startsEntry(const QString &line) const override;
    bool parseEntry(const QStringList &lines, int firstLineNumber, bool mergeContinuations,
                    LogEntry &entry) const override;
    /// The header line is not a record, so the entries are numbered 1..N.
    bool numbersEntriesSequentially() const override { return true; }
};

/// Event Viewer "Save all events as XML" export (`<Events><Event>…`).
///
/// Several events can share one physical line, so the whole document is parsed
/// once to build the entry index (see ILogFormat::buildDocumentEntryIndex).
class WindowsEventXmlFormat : public ILogFormat
{
public:
    QString id() const override { return QStringLiteral("wevt_xml"); }
    QString displayName() const override;
    int probe(const QStringList &sampleLines) const override;
    bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const override;
    bool parseDocument(const QStringList &allLines,
                       std::vector<LogEntry> &outEntries) const override;
    bool parseEntry(const QStringList &lines, int firstLineNumber, bool mergeContinuations,
                    LogEntry &entry) const override;
    /// All events usually share one physical line, so the entries are numbered.
    bool numbersEntriesSequentially() const override { return true; }

private:
    /// Parses one `<Event>…</Event>` element.
    static bool parseEventXml(const QString &xml, LogEntry &entry);
};

} // namespace lv
