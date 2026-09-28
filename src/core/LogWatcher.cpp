#include "core/LogWatcher.h"

#include "core/LogSource.h"

#include <QDir>
#include <QFileInfo>

namespace lv {
namespace {
constexpr int kDefaultIntervalMs = 300;
}

LogWatcher::LogWatcher(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(kDefaultIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &LogWatcher::poll);

    // A file system event triggers an immediate poll; the timer keeps working as
    // a fallback (spec.md REQ-MON-08).
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &LogWatcher::poll);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &LogWatcher::poll);
}

void LogWatcher::start(LogSource *source)
{
    if (m_source == source && m_timer.isActive())
        return;

    stop();
    m_source = source;
    if (!m_source)
        return;

    updateWatchedPaths();
    m_timer.start();
}

void LogWatcher::stop()
{
    m_timer.stop();
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_source = nullptr;
}

void LogWatcher::setInterval(int milliseconds)
{
    m_timer.setInterval(qMax(50, milliseconds));
}

void LogWatcher::updateWatchedPaths()
{
    if (!m_source)
        return;

    const QString path = m_source->documentInfo().filePath;
    if (path.isEmpty())
        return;

    // Watch the file itself and its directory: replacing the file (rotation)
    // only shows up as a directory change on most platforms.
    if (!m_watcher.files().contains(path) && QFileInfo::exists(path))
        m_watcher.addPath(path);

    const QString directory = QFileInfo(path).absolutePath();
    if (!directory.isEmpty() && !m_watcher.directories().contains(directory))
        m_watcher.addPath(directory);
}

void LogWatcher::poll()
{
    if (!m_source)
        return;

    bool rebuilt = false;
    QString error;
    const int before = m_source->rowCount();
    const int appended = m_source->refreshFromDisk(&rebuilt, &error);

    if (!error.isEmpty()) {
        emit watchingFailed(error);
        return;
    }

    // A rotated file may have to be re-added to the watcher.
    updateWatchedPaths();

    if (rebuilt) {
        emit documentRebuilt();
        return;
    }
    if (appended > 0)
        emit rowsAppended(before, appended);
}

} // namespace lv
