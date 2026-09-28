#pragma once

#include "core/LogEntry.h"

#include <QString>
#include <QStringList>

namespace lv {

/// Interface implemented by every log format parser.
///
/// Parsers are stateless and cheap to call; a source owns one parser instance
/// and calls parseEntry() for the entries that are actually displayed.
class ILogFormat
{
public:
    virtual ~ILogFormat() = default;

    /// Stable identifier used in settings, the CLI (--format) and the status bar.
    virtual QString id() const = 0;

    /// Human readable, translated name shown in the UI.
    virtual QString displayName() const = 0;

    /// Returns 0..100: the share of \a sampleLines that this format can parse.
    /// Used by the registry to pick the best format for a file.
    virtual int probe(const QStringList &sampleLines) const = 0;

    /// Parses one physical line. Returns false when the line does not match the
    /// format (the caller then falls back to "whole line is the message").
    virtual bool parseLine(const QString &line, int lineNumber, LogEntry &entry) const = 0;

    /// True when \a line starts a new log entry. The default implementation
    /// treats every line the anchor matches as an entry start, so lines that do
    /// not match (stack traces, wrapped messages) become continuation lines
    /// (spec.md REQ-PARSE-06).
    virtual bool startsEntry(const QString &line) const;

    /// Parses one entry made of its physical \a lines (the first line has the
    /// 1-based number \a firstLineNumber). The default implementation parses the
    /// first line and appends the continuation lines to the message when
    /// \a mergeContinuations is set.
    virtual bool parseEntry(const QStringList &lines, int firstLineNumber,
                            bool mergeContinuations, LogEntry &entry) const;

    /// Block formats (for example the Windows event text export) consume several
    /// lines per entry; declared here so the source can group them correctly.
    virtual bool isBlockFormat() const { return false; }

    /// True when the Line column must number the entries of the source (1, 2,
    /// 3, …) instead of showing the first physical line of each entry.
    ///
    /// Structured exports need this: a header block precedes the first record
    /// ("Level<TAB>Date and Time<TAB>…") or several records share one physical
    /// line (the event log XML export writes the whole document as one line), so
    /// the physical line cannot tell the rows apart (spec.md REQ-PARSE-03).
    virtual bool numbersEntriesSequentially() const { return false; }

    /// Structured documents (an XML export) are parsed as a whole because one
    /// entry is not a range of lines: several events may share one line.
    /// Return true and append the entries (with their first physical line set)
    /// when the format handled the document; the source then serves them as is.
    virtual bool parseDocument(const QStringList &allLines,
                              std::vector<LogEntry> &outEntries) const
    {
        Q_UNUSED(allLines);
        Q_UNUSED(outEntries);
        return false;
    }
};

} // namespace lv
