#pragma once

#include "core/LogLevel.h"

#include <QDateTime>
#include <QHash>
#include <QString>

namespace lv {

/// One logical log entry (may span several physical lines once continuation
/// merging is enabled).
struct LogEntry {
    QDateTime time;                 ///< invalid when the format has no timestamp
    LogLevel level = LogLevel::Other;
    QString rawLevel;               ///< level text exactly as found in the file
    QString thread;                 ///< thread name, when available
    QString target;                 ///< logger name / module / tag
    QString host;                   ///< host name, when available
    QString pid;                    ///< process id, when available
    QHash<QString, QString> extra;  ///< additional key/value columns
    QString message;                ///< full, untruncated message text
    int sourceIndex = 0;            ///< index of the source file in the document
    qint64 firstLine = 0;           ///< Line column value: 1-based first physical
                                    ///< line, or the 1-based entry number when the
                                    ///< format numbers entries (REQ-PARSE-03)
    int physicalLines = 1;          ///< number of physical lines merged into this entry
};

} // namespace lv
