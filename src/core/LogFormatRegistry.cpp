#include "core/LogFormatRegistry.h"

#include "core/formats/ApplicationFormats.h"
#include "core/formats/GenericFormat.h"
#include "core/formats/StructuredFormats.h"
#include "core/formats/SyslogFormats.h"
#include "core/formats/TracingFormat.h"
#include "core/formats/WindowsFormats.h"

#include <QCoreApplication>

namespace lv {
namespace {
constexpr int kMinDetectionScore = 60;
}

LogFormatRegistry::LogFormatRegistry()
{
    m_formats.push_back(std::make_unique<TracingFormat>());
    m_formats.push_back(std::make_unique<Syslog5424Format>());
    m_formats.push_back(std::make_unique<Syslog3164Format>(QStringLiteral("syslog3164"),
                                                           QCoreApplication::translate(
                                                               "Formats", "syslog RFC 3164")));
    m_formats.push_back(std::make_unique<Syslog3164Format>(QStringLiteral("journalctl_short"),
                                                           QCoreApplication::translate(
                                                               "Formats", "journalctl (short)")));
    m_formats.push_back(std::make_unique<JsonLinesFormat>(QStringLiteral("journal_json"),
                                                          QCoreApplication::translate(
                                                              "Formats", "journalctl (JSON)"),
                                                          true));
    m_formats.push_back(std::make_unique<JsonLinesFormat>(QStringLiteral("json_lines"),
                                                          QCoreApplication::translate(
                                                              "Formats", "JSON lines"),
                                                          false));
    m_formats.push_back(std::make_unique<LogfmtFormat>());
    m_formats.push_back(std::make_unique<CsvFormat>());
    m_formats.push_back(std::make_unique<PythonLoggingFormat>());
    m_formats.push_back(std::make_unique<SerilogFormat>());
    m_formats.push_back(std::make_unique<Log4jLogbackFormat>());
    m_formats.push_back(std::make_unique<WindowsEventTextFormat>());
    m_formats.push_back(std::make_unique<WindowsEventTsvFormat>());
    m_formats.push_back(std::make_unique<WindowsEventXmlFormat>());
    m_formats.push_back(std::make_unique<IisW3cFormat>());
    // The generic format must come last: it always matches, but never scores
    // higher than a specialised parser that understands the same lines.
    m_formats.push_back(std::make_unique<GenericFormat>());
}

const LogFormatRegistry &LogFormatRegistry::instance()
{
    static const LogFormatRegistry registry;
    return registry;
}

QVector<const ILogFormat *> LogFormatRegistry::formats() const
{
    QVector<const ILogFormat *> result;
    result.reserve(static_cast<int>(m_formats.size()));
    for (const auto &format : m_formats)
        result.append(format.get());
    return result;
}

const ILogFormat *LogFormatRegistry::findById(const QString &id) const
{
    for (const auto &format : m_formats) {
        if (format->id() == id)
            return format.get();
    }
    return nullptr;
}

const ILogFormat *LogFormatRegistry::detect(const QStringList &sampleLines) const
{
    const ILogFormat *best = nullptr;
    int bestScore = 0;
    for (const auto &format : m_formats) {
        const int score = format->probe(sampleLines);
        if (score > bestScore) {
            bestScore = score;
            best = format.get();
        }
    }
    if (bestScore >= kMinDetectionScore && best && best->id() != QLatin1String("generic"))
        return best;
    return findById(QStringLiteral("generic"));
}

} // namespace lv
