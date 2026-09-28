#include "platform/PlatformInfo.h"

#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QStyleHints>
#include <QSysInfo>

namespace lv::PlatformInfo {
namespace {

QString compilerName()
{
#if defined(_MSC_VER)
    const int major = _MSC_VER / 100;
    const int minor = _MSC_VER % 100;
    return QStringLiteral("MSVC %1.%2").arg(major).arg(minor);
#elif defined(__clang__)
    return QStringLiteral("Clang %1.%2").arg(__clang_major__).arg(__clang_minor__);
#elif defined(__GNUC__)
    return QStringLiteral("GCC %1.%2").arg(__GNUC__).arg(__GNUC_MINOR__);
#else
    return QStringLiteral("unknown compiler");
#endif
}

QString buildTimestamp()
{
    return QStringLiteral(__DATE__) + QLatin1Char(' ') + QStringLiteral(__TIME__);
}

} // namespace

QString osName()
{
    const QString pretty = QSysInfo::prettyProductName();
    return pretty.isEmpty() ? QSysInfo::kernelType() : pretty;
}

QString configDirPath()
{
#if defined(Q_OS_WIN)
    // %APPDATA%\LogViewer (Roaming), matching spec.md CON-11.
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    // $XDG_CONFIG_HOME/LogViewer, defaulting to ~/.config/LogViewer.
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
#endif
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.log-viewer");

    // Guard against Qt adding organisation sub directories when none is wanted.
    QDir dir(base);
    if (!dir.exists())
        QDir().mkpath(base);
    return base;
}

QString settingsFilePath()
{
    return QDir(configDirPath()).filePath(QStringLiteral("settings.ini"));
}

QString monospaceFontFamily()
{
#if defined(Q_OS_WIN)
    const QStringList preferred = {QStringLiteral("Consolas"),
                                   QStringLiteral("Cascadia Mono"),
                                   QStringLiteral("Courier New")};
    const QStringList families = QFontDatabase::families();
    for (const QString &candidate : preferred) {
        if (families.contains(candidate, Qt::CaseInsensitive))
            return candidate;
    }
#endif
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

QFont monospaceFont(int pointSize)
{
    QFont font(monospaceFontFamily(), pointSize);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

bool systemThemeIsDark()
{
    if (const QStyleHints *hints = QGuiApplication::styleHints())
        return hints->colorScheme() == Qt::ColorScheme::Dark;
    return false;
}

QString buildInfo()
{
    return QStringLiteral("%1 · Qt %2 · built %3")
        .arg(compilerName(), QString::fromLatin1(qVersion()), buildTimestamp());
}

} // namespace lv::PlatformInfo
