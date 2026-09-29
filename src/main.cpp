#include "app/CliParser.h"
#include "app/SettingsStore.h"
#include "app/ThemeManager.h"
#include "app/TranslationManager.h"
#include "platform/FileAssociation.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>

#include <cstdio>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

namespace {

/// Window icon: the same artwork as the Linux desktop icon, rasterised at the
/// sizes the window managers request and shipped in the Qt resource file
/// (see resources/resources.qrc).
QIcon applicationIcon()
{
    static constexpr int sizeSteps[] = {16, 24, 32, 48, 64, 128, 256};
    QIcon icon;
    for (const int size : sizeSteps)
        icon.addFile(QStringLiteral(":/icons/log-viewer-%1.png").arg(size));
    return icon;
}

/// Windows GUI applications are not attached to a console; --help/--version
/// still need to print when the program is started from a terminal (REQ-CLI-05).
/// When stdout is already usable (a console or a redirect/pipe from the caller)
/// nothing is changed, so redirected output keeps working.
void attachConsoleIfPossible()
{
#if defined(Q_OS_WIN)
    const HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdoutHandle != nullptr && stdoutHandle != INVALID_HANDLE_VALUE
        && GetFileType(stdoutHandle) != FILE_TYPE_UNKNOWN) {
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS) != 0) {
        FILE *stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
    }
#endif
}

void writeLine(const QString &text, bool toStdErr = false)
{
    const QByteArray utf8 = text.toUtf8() + '\n';
    std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()),
                toStdErr ? stderr : stdout);
    std::fflush(toStdErr ? stderr : stdout);
}

} // namespace

int main(int argc, char *argv[])
{
    attachConsoleIfPossible();

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("LogViewer"));
    QApplication::setApplicationVersion(QStringLiteral("1.2.0"));
    QApplication::setWindowIcon(applicationIcon());
    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        QApplication::setStyle(fusion);

    const lv::CliOptions options = lv::CliParser::parse(QCoreApplication::arguments().mid(1));
    if (!options.error.isEmpty()) {
        writeLine(options.error, true);
        writeLine(lv::CliParser::usageText(), true);
        return 2;
    }
    if (options.helpRequested) {
        writeLine(lv::CliParser::usageText());
        return 0;
    }
    if (options.versionRequested) {
        writeLine(lv::CliParser::versionText());
        return 0;
    }

    lv::SettingsStore settings;
    lv::ThemeManager theme(&settings);
    lv::TranslationManager translations;

    const QString language = options.language.isEmpty() ? settings.language() : options.language;
    translations.setLanguage(language);

    // File association (REQ-ASSOC-06/07): the registry always points at this
    // executable, so the command works from any unpack directory. Console-only
    // operation: no window is created and the exit code reports the outcome.
    if (options.registerAssociation || options.unregisterAssociation) {
        const QString extension = lv::FileAssociation::defaultExtension();
        const lv::FileAssociation::Result result = options.registerAssociation
            ? lv::FileAssociation::registerForCurrentUser(
                  extension, QCoreApplication::applicationFilePath(), options.forceAssociation)
            : lv::FileAssociation::unregisterForCurrentUser(extension);
        for (const QString &warning : result.warnings)
            writeLine(warning, true);
        if (!result.ok) {
            writeLine(result.error, true);
            return 1;
        }
        // The executable path is what the registry now points at; show the same
        // canonical form that was written (long path, native separators).
        const QString displayPath = lv::FileAssociation::canonicalExePath(
            QCoreApplication::applicationFilePath());
        writeLine(options.registerAssociation
                      ? lv::CliParser::associationRegisteredText(extension, displayPath)
                      : lv::CliParser::associationRemovedText(extension));
        return 0;
    }

    if (options.listFormats) {
        writeLine(lv::CliParser::formatListText());
        return 0;
    }

    theme.apply();

    lv::MainWindow window(&settings, &theme, &translations);
    window.show();

    if (options.demo)
        window.loadDemoData();
    else if (!options.files.isEmpty())
        window.openPaths(options.files, options.formatId);

    // Layout self check (used by the UI verification script).
    const QByteArray layoutDumpPath = qgetenv("LOGVIEWER_DUMP_LAYOUT");
    if (!layoutDumpPath.isEmpty()) {
        const QString path = QString::fromLocal8Bit(layoutDumpPath);
        int delay = 1200;
        if (const QByteArray requested = qgetenv("LOGVIEWER_DUMP_LAYOUT_DELAY"); !requested.isEmpty())
            delay = requested.toInt();
        // Dump and quit in one step: the report walks every row (content height
        // checks), so a fixed grace period would truncate it on large documents.
        QTimer::singleShot(delay, &window, [&window, path] {
            window.dumpLayout(path);
            QApplication::quit();
        });
    }

    return QApplication::exec();
}
