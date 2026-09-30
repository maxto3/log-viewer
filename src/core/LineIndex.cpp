#include "core/LineIndex.h"

#include <QFile>
#include <QFileInfo>

#include <array>
#include <limits>

namespace lv {
namespace {
constexpr int kChunkSize = 64 * 1024;

qint32 clampLength(qint64 length)
{
    return static_cast<qint32>(qMin<qint64>(qMax<qint64>(length, 0), std::numeric_limits<qint32>::max()));
}
} // namespace

bool LineIndex::build(const QString &path, int maxLines, QString *errorMessage)
{
    m_starts.clear();
    m_lengths.clear();
    m_maxLineLength = 0;
    m_truncated = false;
    m_fileSize = 0;
    m_indexedBytes = 0;
    m_lastLineIncomplete = false;
    m_skipLeadingLf = false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot open '%1': %2").arg(path, file.errorString());
        return false;
    }
    m_fileSize = file.size();
    if (m_fileSize == 0)
        return true;

    if (!scan(file, 0, maxLines, errorMessage))
        return false;
    indexTrailingPartialLine(file);
    return true;
}

bool LineIndex::refresh(const QString &path, int maxLines, QString *errorMessage, bool *rebuilt)
{
    if (rebuilt)
        *rebuilt = false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot open '%1': %2").arg(path, file.errorString());
        return false;
    }

    const qint64 size = file.size();
    if (size < m_indexedBytes) {
        // Truncated or replaced by a rotated file: start over.
        m_starts.clear();
        m_lengths.clear();
        m_maxLineLength = 0;
        m_truncated = false;
        m_indexedBytes = 0;
        m_lastLineIncomplete = false;
        m_skipLeadingLf = false;
        m_fileSize = size;
        if (!scan(file, 0, maxLines, errorMessage))
            return false;
        indexTrailingPartialLine(file);
        if (rebuilt)
            *rebuilt = true;
        return true;
    }

    if (size == m_fileSize && !m_lastLineIncomplete)
        return true;    // nothing new

    // The previously indexed trailing line (no terminator yet) is re-read once
    // its terminator has been written.
    if (m_lastLineIncomplete && !m_starts.empty()) {
        m_indexedBytes = m_starts.back();
        m_starts.pop_back();
        m_lengths.pop_back();
        m_lastLineIncomplete = false;
    }

    if (!scan(file, m_indexedBytes, maxLines, errorMessage))
        return false;
    indexTrailingPartialLine(file);
    return true;
}

bool LineIndex::scan(QFile &file, qint64 startOffset, int maxLines, QString *errorMessage)
{
    const int lineBudget = maxLines > 0 ? maxLines : std::numeric_limits<int>::max();
    if (m_starts.capacity() == 0)
        m_starts.reserve(1024);

    if (startOffset > 0 && !file.seek(startOffset)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot seek in '%1'").arg(file.fileName());
        return false;
    }

    std::array<char, kChunkSize> buffer{};
    qint64 offset = startOffset;
    qint64 lineStart = startOffset;
    // A '\r' ends a line immediately; the LF of a CRLF pair is swallowed when it
    // arrives afterwards. The state survives chunk and refresh boundaries
    // (m_skipLeadingLf), so a writer that flushes "\r" and "\n" separately is
    // still indexed as one terminator.
    bool skipLeadingLf = m_skipLeadingLf;
    bool done = false;

    const auto endLine = [&](qint64 terminatorOffset) {
        const qint64 length = terminatorOffset - lineStart;
        m_starts.push_back(lineStart);
        m_lengths.push_back(clampLength(length));
        m_maxLineLength = qMax(m_maxLineLength, static_cast<int>(length));
        m_indexedBytes = terminatorOffset + 1;
        lineStart = terminatorOffset + 1;
    };

    while (!done) {
        const qint64 read = file.read(buffer.data(), buffer.size());
        if (read <= 0)
            break;
        for (qint64 i = 0; i < read; ++i) {
            const char c = buffer[static_cast<size_t>(i)];
            const qint64 absolute = offset + i;
            if (skipLeadingLf) {
                skipLeadingLf = false;
                if (c == '\n') {
                    // Second half of a CRLF pair whose CR already ended the line.
                    lineStart = absolute + 1;
                    m_indexedBytes = absolute + 1;
                    continue;
                }
            }
            if (c == '\n' || c == '\r') {
                endLine(absolute);
                skipLeadingLf = (c == '\r');
                if (static_cast<int>(m_starts.size()) >= lineBudget) {
                    m_truncated = true;
                    done = true;
                    break;
                }
            }
        }
        offset += read;
        if (read < static_cast<qint64>(buffer.size()))
            break;
    }

    m_skipLeadingLf = skipLeadingLf;
    return true;
}

void LineIndex::indexTrailingPartialLine(QFile &file)
{
    m_lastLineIncomplete = false;
    if (m_truncated)
        return;     // the line limit was reached; do not index beyond it
    if (m_indexedBytes >= m_fileSize)
        return;     // the file ends with a line terminator

    if (!file.seek(m_indexedBytes))
        return;
    const qint64 remaining = m_fileSize - m_indexedBytes;
    if (remaining <= 0)
        return;

    const QByteArray tail = file.read(remaining);
    qint64 length = tail.size();
    if (length > 0 && tail.endsWith('\r'))
        --length;

    m_starts.push_back(m_indexedBytes);
    m_lengths.push_back(clampLength(length));
    m_maxLineLength = qMax(m_maxLineLength, static_cast<int>(length));
    m_lastLineIncomplete = true;
}

} // namespace lv
