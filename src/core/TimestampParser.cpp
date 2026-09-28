#include "core/TimestampParser.h"

#include <QRegularExpression>
#include <QStringList>
#include <QTimeZone>

namespace lv {
namespace {

const QRegularExpression &isoPattern()
{
    // YYYY-MM-DD[T ]H[H]:MM:SS[,.fraction][Z|±HH:MM|±HHMM]  (single digit hour is
    // used by the Windows event log text export).
    static const QRegularExpression re(QStringLiteral(
        R"(^(\d{4})-(\d{2})-(\d{2})[T ](\d{1,2}):(\d{2}):(\d{2})(?:[.,](\d{1,9}))?\s*(Z|[+-]\d{2}:?\d{2})?$)"));
    return re;
}

const QRegularExpression &syslogPattern()
{
    // Mmm dd HH:MM:SS   (English month names on purpose: independent of locale)
    static const QRegularExpression re(
        QStringLiteral(R"(^([A-Z][a-z]{2})\s+(\d{1,2})\s+(\d{2}):(\d{2}):(\d{2})$)"));
    return re;
}

const QRegularExpression &timeOnlyPattern()
{
    static const QRegularExpression re(
        QStringLiteral(R"(^(\d{2}):(\d{2}):(\d{2})(?:[.,](\d{1,9}))?$)"));
    return re;
}

const QStringList &monthNames()
{
    static const QStringList names = {QStringLiteral("Jan"), QStringLiteral("Feb"),
                                      QStringLiteral("Mar"), QStringLiteral("Apr"),
                                      QStringLiteral("May"), QStringLiteral("Jun"),
                                      QStringLiteral("Jul"), QStringLiteral("Aug"),
                                      QStringLiteral("Sep"), QStringLiteral("Oct"),
                                      QStringLiteral("Nov"), QStringLiteral("Dec")};
    return names;
}

int millisecondsFromFraction(const QString &fraction)
{
    if (fraction.isEmpty())
        return 0;
    QString digits = fraction.left(3);
    while (digits.size() < 3)
        digits.append(u'0');
    return digits.toInt();
}

QTimeZone zoneFromOffsetToken(const QString &token)
{
    if (token.isEmpty())
        return QTimeZone();                     // invalid -> caller uses local time
    if (token == QLatin1String("Z") || token == QLatin1String("z"))
        return QTimeZone::UTC;
    const bool negative = token.startsWith(u'-');
    const QString digits = token.mid(1).remove(u':');
    if (digits.size() != 4)
        return QTimeZone();
    const int hours = digits.left(2).toInt();
    const int minutes = digits.right(2).toInt();
    const int seconds = (hours * 3600 + minutes * 60) * (negative ? -1 : 1);
    return QTimeZone::fromSecondsAheadOfUtc(seconds);
}

} // namespace

QDateTime parseTimestamp(QStringView text, const QDate &fallbackDate)
{
    const QString input = text.toString().trimmed();
    if (input.isEmpty())
        return {};

    QRegularExpressionMatch match = isoPattern().match(input);
    if (match.hasMatch()) {
        const QDate date(match.captured(1).toInt(), match.captured(2).toInt(),
                         match.captured(3).toInt());
        const QTime time(match.captured(4).toInt(), match.captured(5).toInt(),
                         match.captured(6).toInt(), millisecondsFromFraction(match.captured(7)));
        if (!date.isValid() || !time.isValid())
            return {};
        const QTimeZone zone = zoneFromOffsetToken(match.captured(8));
        if (zone.isValid())
            return QDateTime(date, time, zone);
        return QDateTime(date, time);           // no offset -> local time
    }

    match = syslogPattern().match(input);
    if (match.hasMatch()) {
        const int month = monthNames().indexOf(match.captured(1)) + 1;
        const int day = match.captured(2).toInt();
        const QDate reference = fallbackDate.isValid() ? fallbackDate : QDate::currentDate();
        QDate date(reference.year(), month, day);
        // A syslog line without a year that is "in the future" belongs to the
        // previous year (log rotation across a year boundary).
        if (date.isValid() && date > reference.addDays(1))
            date = QDate(reference.year() - 1, month, day);
        const QTime time(match.captured(3).toInt(), match.captured(4).toInt(),
                         match.captured(5).toInt());
        if (!date.isValid() || !time.isValid())
            return {};
        return QDateTime(date, time);
    }

    match = timeOnlyPattern().match(input);
    if (match.hasMatch()) {
        const QDate date = fallbackDate.isValid() ? fallbackDate : QDate::currentDate();
        const QTime time(match.captured(1).toInt(), match.captured(2).toInt(),
                         match.captured(3).toInt(), millisecondsFromFraction(match.captured(4)));
        if (!time.isValid())
            return {};
        return QDateTime(date, time);
    }

    return {};
}

QDate dateFromFileName(const QString &fileName)
{
    static const QRegularExpression re(QStringLiteral(R"((\d{4})-(\d{2})-(\d{2}))"));
    const QRegularExpressionMatch match = re.match(fileName);
    if (!match.hasMatch())
        return {};
    const QDate date(match.captured(1).toInt(), match.captured(2).toInt(),
                     match.captured(3).toInt());
    return date.isValid() ? date : QDate();
}

} // namespace lv
