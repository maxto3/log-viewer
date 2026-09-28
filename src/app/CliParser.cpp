#include "app/CliParser.h"

#include "core/LogFormatRegistry.h"
#include "platform/PlatformInfo.h"

#include <QCoreApplication>

namespace lv {

CliOptions CliParser::parse(const QStringList &arguments)
{
    CliOptions options;
    QString pending;   // option waiting for its value ("--lang" / "--format")

    for (const QString &argument : arguments) {
        if (!pending.isEmpty()) {
            if (pending == QLatin1String("--lang")) {
                options.language = argument;
            } else {
                options.formatId = argument;
                if (argument.compare(QLatin1String("list"), Qt::CaseInsensitive) == 0)
                    options.listFormats = true;
            }
            pending.clear();
            continue;
        }
        if (argument == QLatin1String("-h") || argument == QLatin1String("--help")) {
            options.helpRequested = true;
        } else if (argument == QLatin1String("-v") || argument == QLatin1String("--version")) {
            options.versionRequested = true;
        } else if (argument == QLatin1String("--monitor")) {
            options.monitor = true;
        } else if (argument == QLatin1String("--demo")) {
            options.demo = true;
        } else if (argument == QLatin1String("--lang")) {
            pending = QStringLiteral("--lang");
        } else if (argument.startsWith(QLatin1String("--lang="))) {
            options.language = argument.mid(7);
        } else if (argument == QLatin1String("--format")) {
            pending = QStringLiteral("--format");
        } else if (argument.startsWith(QLatin1String("--format="))) {
            options.formatId = argument.mid(9);
            if (options.formatId.compare(QLatin1String("list"), Qt::CaseInsensitive) == 0)
                options.listFormats = true;
        } else if (argument.startsWith(QLatin1Char('-')) && argument.size() > 1) {
            options.error = tr("Unknown option: %1").arg(argument);
        } else {
            options.files.append(argument);
        }
        if (!options.error.isEmpty())
            return options;
    }

    if (!pending.isEmpty()) {
        options.error = pending == QLatin1String("--lang")
            ? tr("Option --lang requires a value (en or zh_CN).")
            : tr("Option --format requires a value (a format id or 'auto').");
        return options;
    }

    if (!options.language.isEmpty() && options.language != QLatin1String("en")
        && options.language != QLatin1String("zh_CN")) {
        options.error = tr("Unsupported language '%1' (supported: en, zh_CN).").arg(options.language);
    }
    return options;
}

QString CliParser::usageText()
{
    return tr(
               "Usage: log-viewer [options] [files...]\n"
               "\n"
               "Opens one or more log files. Files given at the same time are merged\n"
               "into a single view when they share the same format.\n"
               "\n"
               "Options:\n"
               "  -h, --help             Show this help and exit\n"
               "  -v, --version          Show version and build information and exit\n"
               "      --lang <en|zh_CN>  Override the interface language\n"
               "      --format <id>      Force a log format ('auto' for detection,\n"
               "                         'list' to print the available ids)\n"
               "      --monitor          Enable live monitoring after opening one file\n"
               "      --demo             Load built-in demo data (UI preview only)\n");
}

QString CliParser::versionText()
{
    return QStringLiteral("log-viewer %1\n%2\n%3")
        .arg(QCoreApplication::applicationVersion(), PlatformInfo::buildInfo(),
             PlatformInfo::osName());
}

QString CliParser::formatListText()
{
    QStringList lines;
    const auto formats = LogFormatRegistry::instance().formats();
    for (const ILogFormat *format : formats)
        lines.append(QStringLiteral("  %1\t%2").arg(format->id(), format->displayName()));
    return tr("Available log formats:") + QLatin1Char('\n') + lines.join(QLatin1Char('\n'));
}

} // namespace lv
