#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

namespace lv {

/// Parses the timestamp shapes commonly found in log files.
///
/// Supported inputs:
///  * ISO 8601 with optional fractional seconds (up to 9 digits) and an
///    optional "Z" or "+HH:MM" / "+HHMM" offset,
///  * "YYYY-MM-DD HH:MM:SS[.mmm]" / "YYYY-MM-DDTHH:MM:SS",
///  * syslog "Mmm dd HH:MM:SS" (year taken from \a fallbackDate),
///  * time only "HH:MM:SS[.mmm]" (date taken from \a fallbackDate).
///
/// Returns an invalid QDateTime when nothing matches.
QDateTime parseTimestamp(QStringView text, const QDate &fallbackDate = QDate());

/// Extracts a date in the form YYYY-MM-DD from a file name, e.g.
/// "sslocal.2026-09-28.log" -> 2026-09-28. Returns an invalid QDate on failure.
QDate dateFromFileName(const QString &fileName);

} // namespace lv
