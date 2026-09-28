#pragma once

#include <QString>

namespace lv {

/// Logical table columns. The order in this enum is the preferred display
/// order; the actual column set is derived from the log format and content.
enum class ColumnKind {
    Time,
    Level,
    Thread,
    Target,
    Pid,
    Host,
    File,
    Line,
    Message,
    Extra
};

/// Stable, language independent key of a column kind. Used by the Columns menu
/// and by the persisted column visibility (REQ-TABLE-11); extra columns add
/// their own key ("extra:<name>").
inline QString columnKindKey(ColumnKind kind)
{
    switch (kind) {
    case ColumnKind::Time:    return QStringLiteral("time");
    case ColumnKind::Level:   return QStringLiteral("level");
    case ColumnKind::Thread:  return QStringLiteral("thread");
    case ColumnKind::Target:  return QStringLiteral("target");
    case ColumnKind::Pid:     return QStringLiteral("pid");
    case ColumnKind::Host:    return QStringLiteral("host");
    case ColumnKind::File:    return QStringLiteral("file");
    case ColumnKind::Line:    return QStringLiteral("line");
    case ColumnKind::Message: return QStringLiteral("message");
    case ColumnKind::Extra:   break;
    }
    return QStringLiteral("extra");
}

} // namespace lv
