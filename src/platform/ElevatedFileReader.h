#pragma once

#include <QString>

namespace lv {

/// Elevated read support (spec.md REQ-REL-04).
///
/// On Linux an unreadable file can be opened through the system authentication
/// helper (`pkexec`/polkit): the helper process runs as root, reads the file and
/// streams the raw bytes to stdout. This (unprivileged) process writes them into
/// a private snapshot owned by the current user. The elevated process never
/// writes to a user path and no shell is involved (REQ-PLAT-08 exception).
///
/// On other platforms, and on Linux without `pkexec`, the API degrades to
/// isSupported() == false and error/no-op results.
namespace ElevatedFileReader {

/// True when this build and environment can perform an elevated read.
bool isSupported();

/// Localised explanation why an elevated read is not possible and how to grant
/// read access manually (empty when isSupported()).
QString unsupportedText();

/// Streams \a path through the authentication helper and stores the bytes in a
/// private snapshot in the per-user runtime directory (fallback: temp).
/// Returns the snapshot path, or an empty string with \a errorMessage set. The
/// caller owns the snapshot and releases it with removeSnapshot().
/// \a programOverride replaces the pkexec discovery (test seam).
QString createSnapshot(const QString &path, QString *errorMessage,
                       const QString &programOverride = QString());

/// Deletes a snapshot created by createSnapshot(): the file and its private
/// directory (only when empty). No-op for empty or unknown paths.
void removeSnapshot(const QString &snapshotPath);

/// Internal helper entry point (runs as root via pkexec): writes the raw bytes
/// of \a path to stdout. Returns the process exit code (REQ-CLI-11).
int runStreamHelper(const QString &path);

/// Test seam: forces the program used by createSnapshot() (empty = pkexec).
void setProgramOverrideForTesting(const QString &program);

} // namespace ElevatedFileReader

} // namespace lv
