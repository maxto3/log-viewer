#include "app/TranslationManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QWidget>

namespace lv {
namespace {

const char *kTranslationPrefix = "logviewer_";

QStringList candidatePaths(const QString &code)
{
    const QString fileName = QLatin1String(kTranslationPrefix) + code + QLatin1String(".qm");
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList paths;
    paths << QStringLiteral(":/i18n/") + fileName;
    paths << QDir(appDir).filePath(QStringLiteral("translations/") + fileName);
    paths << QDir(appDir).filePath(fileName);
    paths << QDir(appDir).filePath(QStringLiteral("../translations/") + fileName);
#if !defined(Q_OS_WIN)
    // System installation layout (see the CPack DEB configuration).
    paths << QStringLiteral("/usr/share/log-viewer/translations/") + fileName;
    paths << QStringLiteral("/usr/local/share/log-viewer/translations/") + fileName;
#endif
    return paths;
}

} // namespace

TranslationManager::TranslationManager(QObject *parent)
    : QObject(parent)
{
}

QStringList TranslationManager::availableLanguages()
{
    return {QStringLiteral("en"), QStringLiteral("zh_CN")};
}

QString TranslationManager::displayName(const QString &code)
{
    if (code == QLatin1String("zh_CN"))
        return QStringLiteral("简体中文");
    return QStringLiteral("English");
}

bool TranslationManager::setLanguage(const QString &code)
{
    const QString target = code.isEmpty() ? QStringLiteral("en") : code;

    if (m_installed) {
        QCoreApplication::removeTranslator(&m_translator);
        m_installed = false;
    }
    if (m_qtInstalled) {
        QCoreApplication::removeTranslator(&m_qtTranslator);
        m_qtInstalled = false;
    }

    if (target == QLatin1String("en")) {
        m_language = target;
        notifyWidgets();
        emit languageChanged(m_language);
        return true;
    }

    bool loaded = false;
    for (const QString &path : candidatePaths(target)) {
        if (path.startsWith(QLatin1String(":/")) || QFileInfo::exists(path)) {
            if (m_translator.load(path)) {
                loaded = true;
                break;
            }
        }
    }
    if (loaded) {
        QCoreApplication::installTranslator(&m_translator);
        m_installed = true;
    }

    // Qt's own strings (file dialogs, message boxes, font dialog) come from the
    // qtbase_<locale>.qm / qt_<locale>.qm catalogues shipped with Qt.
    const QStringList qtFileNames = {QStringLiteral("qtbase_") + target,
                                     QStringLiteral("qt_") + target};
    QStringList qtDirs;
    qtDirs << QLibraryInfo::path(QLibraryInfo::TranslationsPath)
           << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations"))
           << QCoreApplication::applicationDirPath();
    bool qtLoaded = false;
    for (const QString &fileName : qtFileNames) {
        for (const QString &dir : qtDirs) {
            if (dir.isEmpty())
                continue;
            if (m_qtTranslator.load(fileName, dir)) {
                qtLoaded = true;
                break;
            }
        }
        if (qtLoaded)
            break;
    }
    if (qtLoaded) {
        QCoreApplication::installTranslator(&m_qtTranslator);
        m_qtInstalled = true;
    }

    m_language = target;
    notifyWidgets();
    emit languageChanged(m_language);
    return loaded;
}

void TranslationManager::notifyWidgets()
{
    QEvent event(QEvent::LanguageChange);
    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget *widget : widgets)
        QCoreApplication::sendEvent(widget, &event);
}

} // namespace lv
