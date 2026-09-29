#pragma once

#include "core/IEntryProvider.h"
#include "core/LineIndex.h"

#include <QDate>
#include <QFile>
#include <QHash>

#include <memory>
#include <vector>

namespace lv {

class ILogFormat;

/// File-backed log source.
///
/// Two indexes are built:
///  * a physical line index (byte offsets, see LineIndex);
///  * an entry index that groups physical lines into log entries: continuation
///    lines (stack traces, wrapped messages) are merged into the previous entry
///    when enabled, block formats (Windows event text) consume several lines.
///
/// Entries are parsed on demand, so opening a large file stays fast and memory
/// bounded.
class LogSource : public IEntryProvider
{
public:
    /// Opens \a path, auto-detects the log format (unless \a forcedFormatId is
    /// set) and returns the source, or nullptr with \a errorMessage set.
    static std::shared_ptr<LogSource> open(const QString &path,
                                           const QString &forcedFormatId,
                                           QString *errorMessage,
                                           int maxLines = 2'000'000,
                                           bool mergeContinuations = true);

    int rowCount() const override;
    const LogEntry &entryAt(int row) const override;
    QVector<int> levelCounts() const override;
    DocumentInfo documentInfo() const override;
    QString sourceName(int sourceIndex) const override;

    const ILogFormat *format() const { return m_format; }

    /// Decoded text of one physical line.
    QString readLine(int row, bool *ok = nullptr) const;

    /// Raw (undecoded) bytes of one physical line, used for the detail pane.
    QByteArray readRawLine(int row) const;

    /// Re-reads the file and appends lines/entries written since the last scan.
    /// Returns the number of appended entries; \a rebuilt is set when the file
    /// was truncated or rotated (the whole document changed and the caller
    /// should reset its model).
    int refreshFromDisk(bool *rebuilt, QString *errorMessage);

    /// Drops the cached level histogram (used after a document rebuild).
    void invalidateLevelCounts();

    /// Keeps the file open for a multi line pass so it does not have to be
    /// reopened for every line (that dominated the open time of large files).
    /// \c endBulkRead() releases the handle again: between passes no handle is
    /// held, so log rotation (rename/replace) keeps working on Windows.
    void beginBulkRead() const;
    void endBulkRead() const;

private:
    LogSource() = default;

    void detectEncoding();
    QString decodeBytes(const QByteArray &raw) const;
    void parseEntryAt(int row, LogEntry &entry) const;
    void buildEntryIndex(int fromPhysicalLine, int maxLines);
    QDate fallbackDate() const;

    std::unique_ptr<LineIndex> m_index;
    const ILogFormat *m_format = nullptr;
    QString m_path;
    QString m_fileName;

    enum class Encoding { Utf8, Utf16LE, Utf16BE };
    Encoding m_encoding = Encoding::Utf8;

    QDate m_fileNameDate;
    QDate m_modifiedDate;
    int m_maxLines = 2'000'000;
    bool m_mergeContinuations = true;
    QString m_legacyEncoding;   ///< detected non-UTF-8 encoding (empty = UTF-8)

    /// Entries of a structured document (event log XML); empty for line based
    /// formats.
    std::vector<LogEntry> m_documentEntries;

    /// Entry index: first physical line (0-based) and line count per entry.
    std::vector<int> m_entryStart;
    std::vector<qint16> m_entryLines;

    mutable QHash<int, LogEntry> m_cache;
    mutable QVector<int> m_levelCounts;
    mutable bool m_countsComputed = false;
    mutable bool m_decodingFallbackUsed = false;
    /// Handle used only inside a beginBulkRead()/endBulkRead() pass.
    mutable std::unique_ptr<QFile> m_bulkFile;
    mutable bool m_bulkOpen = false;

    static constexpr int kMaxCachedEntries = 20'000;
    static constexpr int kMaxCountScanLines = 300'000;
    /// Files up to this many physical lines may be parsed as one document.
    static constexpr int kMaxDocumentIndexLines = 400'000;
    /// Upper bound for entries of a parsed document (memory protection).
    static constexpr int kMaxDocumentEntries = 100'000;
};

} // namespace lv
