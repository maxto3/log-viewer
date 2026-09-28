#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace lv {

class LogSource;

/// Live monitoring (tail -f) for a single log file.
///
/// A 300 ms poller is the primary mechanism (it also covers file systems where
/// the watcher cannot report changes); QFileSystemWatcher adds an immediate
/// reaction when the platform reports a modification. Appended lines are only
/// re-indexed, never the whole file.
class LogWatcher : public QObject
{
    Q_OBJECT

public:
    explicit LogWatcher(QObject *parent = nullptr);

    /// Starts watching \a source. The source is not owned and must outlive the
    /// watcher.
    void start(LogSource *source);
    void stop();
    bool isActive() const { return m_source != nullptr; }

    /// Poll interval in milliseconds (default 300, as specified in design-doc §11).
    void setInterval(int milliseconds);
    int interval() const { return m_timer.interval(); }

    /// Performs one update cycle immediately (also used by the tests).
    void poll();

signals:
    /// \a count rows were appended starting at \a firstRow.
    void rowsAppended(int firstRow, int count);
    /// The file was truncated or rotated and was re-read from scratch.
    void documentRebuilt();
    /// The file could not be read any more.
    void watchingFailed(const QString &message);

private:
    void updateWatchedPaths();

    QTimer m_timer;
    QFileSystemWatcher m_watcher;
    LogSource *m_source = nullptr;
};

} // namespace lv
