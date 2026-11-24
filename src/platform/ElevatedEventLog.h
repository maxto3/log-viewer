#pragma once

#include "core/IEventLogBackend.h"

#include <QString>

namespace lv::ElevatedEventLog {

/// True when this build can start the native UAC helper (Windows only).
bool isSupported();

/// Localised explanation why elevated event log reads are unavailable.
QString unsupportedText();

/// Reads \a channel (non-empty) or \a path through an elevated helper process
/// and returns the records. \a maxRecords <= 0 loads every record. On failure
/// \a result.error is set and \a error receives the same text.
EventLogReadResult read(const QString &channel, const QString &path, int maxRecords,
                        QString *error);

/// Internal helper entry point (`--event-log-stream`, REQ-CLI-12). Must run
/// before any GUI initialisation; returns the process exit code.
int runStreamHelper();

/// Test seam: launch the helper without UAC (`runas` is the production path).
void setUseRunasForTesting(bool useRunas);

/// Test seam: use \a program instead of this executable as the helper.
void setProgramOverrideForTesting(const QString &program);

} // namespace lv::ElevatedEventLog
