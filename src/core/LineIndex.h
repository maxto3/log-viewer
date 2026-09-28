#pragma once

#include <QString>
#include <vector>

class QFile;

namespace lv {

/// Byte offsets of every physical line in a file.
///
/// Building the index only scans bytes; it never holds file content in memory,
/// which keeps the footprint at ~12 bytes per line even for very large files.
/// refresh() appends the lines that were written since the last scan, so live
/// monitoring (tail -f) does not have to re-read the whole file.
class LineIndex
{
public:
    /// Scans \a path and records the start offset and byte length of each line.
    /// Stops after \a maxLines lines (isTruncated() becomes true) or when the
    /// file cannot be read (\a errorMessage is set).
    bool build(const QString &path, int maxLines, QString *errorMessage);

    /// Re-scans \a path and appends newly written lines. When the file shrank or
    /// was replaced the index is rebuilt from scratch and \a rebuilt is set.
    /// Returns false with \a errorMessage set when the file cannot be read.
    bool refresh(const QString &path, int maxLines, QString *errorMessage, bool *rebuilt);

    int lineCount() const { return static_cast<int>(m_starts.size()); }
    qint64 lineStart(int line) const { return m_starts[static_cast<size_t>(line)]; }
    int lineLength(int line) const { return m_lengths[static_cast<size_t>(line)]; }
    qint64 fileSize() const { return m_fileSize; }
    int maxLineLength() const { return m_maxLineLength; }
    bool isTruncated() const { return m_truncated; }

private:
    /// Scans complete (newline terminated) lines starting at \a startOffset.
    bool scan(QFile &file, qint64 startOffset, int maxLines, QString *errorMessage);
    /// Indexes a final line that has no line terminator yet (a writer in
    /// progress). Such a line is dropped and re-read on the next refresh.
    void indexTrailingPartialLine(QFile &file);

    std::vector<qint64> m_starts;   ///< byte offset of the first character of each line
    std::vector<qint32> m_lengths;  ///< byte length excluding the line terminator
    qint64 m_fileSize = 0;
    qint64 m_indexedBytes = 0;      ///< offset directly after the last complete line
    int m_maxLineLength = 0;
    bool m_truncated = false;
    bool m_lastLineIncomplete = false;
};

} // namespace lv
