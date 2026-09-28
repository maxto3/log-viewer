#include "app/SettingsStore.h"

#include "platform/PlatformInfo.h"

#include <QApplication>
#include <QSettings>

namespace lv {
namespace {

constexpr int kMaxRecentFiles = 10;
constexpr int kDefaultMaxLinesPerFile = 2'000'000;

const char *kThemeLight = "light";
const char *kThemeDark = "dark";
const char *kThemeSystem = "system";

} // namespace

SettingsStore::SettingsStore(const QString &filePath, QObject *parent)
    : QObject(parent)
{
    const QString path = filePath.isEmpty() ? PlatformInfo::settingsFilePath() : filePath;
    m_settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
    migrateLegacyKeys();
}

void SettingsStore::migrateLegacyKeys()
{
    // The details pane switch was renamed twice during development: accept the
    // older spellings once and drop them afterwards.
    if (m_settings->contains(QStringLiteral("view/showDetailsPane")))
        return;

    const QStringList legacyKeys = {QStringLiteral("view/alwaysShowDetails"),
                                    QStringLiteral("view/detailsOnStartup"),
                                    QStringLiteral("view/detailsOnRowClick")};
    bool foundLegacy = false;
    bool enabled = false;
    for (const QString &key : legacyKeys) {
        if (m_settings->contains(key)) {
            foundLegacy = true;
            if (key != QLatin1String("view/detailsOnRowClick"))
                enabled = m_settings->value(key).toBool();
            break;
        }
    }
    if (!foundLegacy)
        return;

    m_settings->setValue(QStringLiteral("view/showDetailsPane"), enabled);
    for (const QString &key : legacyKeys)
        m_settings->remove(key);
}

SettingsStore::~SettingsStore() = default;

QString SettingsStore::filePath() const
{
    return m_settings->fileName();
}

QString SettingsStore::language() const
{
    return m_settings->value(QStringLiteral("language"), QStringLiteral("en")).toString();
}

void SettingsStore::setLanguage(const QString &code)
{
    m_settings->setValue(QStringLiteral("language"), code);
}

QFont SettingsStore::interfaceFont() const
{
    const QVariant value = m_settings->value(QStringLiteral("font/interface"));
    if (value.canConvert<QFont>()) {
        const QFont font = value.value<QFont>();
        if (!font.family().isEmpty())
            return font;
    }
    return QApplication::font();
}

void SettingsStore::setInterfaceFont(const QFont &font)
{
    m_settings->setValue(QStringLiteral("font/interface"), font);
    emit fontsChanged();
}

QFont SettingsStore::tableFont() const
{
    const QVariant value = m_settings->value(QStringLiteral("font/table"));
    if (value.canConvert<QFont>()) {
        const QFont font = value.value<QFont>();
        if (!font.family().isEmpty())
            return font;
    }
    return PlatformInfo::monospaceFont(10);
}

void SettingsStore::setTableFont(const QFont &font)
{
    m_settings->setValue(QStringLiteral("font/table"), font);
    emit fontsChanged();
}

QFont SettingsStore::headerFont() const
{
    QFont font;
    const QVariant value = m_settings->value(QStringLiteral("font/header"));
    if (value.canConvert<QFont>()) {
        font = value.value<QFont>();
        if (font.family().isEmpty())
            font = tableFont();
    } else {
        font = tableFont();
    }
    font.setBold(true);
    return font;
}

void SettingsStore::setHeaderFont(const QFont &font)
{
    QFont stored = font;
    stored.setBold(true);
    m_settings->setValue(QStringLiteral("font/header"), stored);
    emit fontsChanged();
}

void SettingsStore::resetFonts()
{
    m_settings->remove(QStringLiteral("font"));
    emit fontsChanged();
}

SettingsStore::Theme SettingsStore::theme() const
{
    return themeFromString(
        m_settings->value(QStringLiteral("appearance/theme"), QLatin1String(kThemeLight)).toString());
}

void SettingsStore::setTheme(Theme theme)
{
    m_settings->setValue(QStringLiteral("appearance/theme"), themeToString(theme));
    emit themeChanged();
}

QString SettingsStore::syntaxTheme() const
{
    return m_settings->value(QStringLiteral("appearance/syntaxTheme"), QStringLiteral("follow"))
        .toString();
}

void SettingsStore::setSyntaxTheme(const QString &themeId)
{
    m_settings->setValue(QStringLiteral("appearance/syntaxTheme"), themeId);
    emit themeChanged();
}

SettingsStore::DetailsPosition SettingsStore::detailsPosition() const
{
    const QString value =
        m_settings->value(QStringLiteral("layout/detailsPosition"), QStringLiteral("right")).toString();
    return value == QLatin1String("bottom") ? DetailsPosition::Bottom : DetailsPosition::Right;
}

void SettingsStore::setDetailsPosition(DetailsPosition position)
{
    m_settings->setValue(QStringLiteral("layout/detailsPosition"),
                         position == DetailsPosition::Bottom ? QStringLiteral("bottom")
                                                             : QStringLiteral("right"));
    emit detailsPositionChanged();
}

bool SettingsStore::showDetailsPane() const
{
    return m_settings->value(QStringLiteral("view/showDetailsPane"), false).toBool();
}

void SettingsStore::setShowDetailsPane(bool enabled)
{
    m_settings->setValue(QStringLiteral("view/showDetailsPane"), enabled);
    emit detailsBehaviorChanged();
}

QColor SettingsStore::highlightBackground() const
{
    const QColor color(
        m_settings->value(QStringLiteral("appearance/highlightBg"), QStringLiteral("#7CFC00")).toString());
    return color.isValid() ? color : QColor(QStringLiteral("#7CFC00"));
}

void SettingsStore::setHighlightBackground(const QColor &color)
{
    m_settings->setValue(QStringLiteral("appearance/highlightBg"), color.name());
    emit highlightColorsChanged();
}

QColor SettingsStore::highlightForeground() const
{
    const QColor color(
        m_settings->value(QStringLiteral("appearance/highlightFg"), QStringLiteral("#000000")).toString());
    return color.isValid() ? color : QColor(QStringLiteral("#000000"));
}

void SettingsStore::setHighlightForeground(const QColor &color)
{
    m_settings->setValue(QStringLiteral("appearance/highlightFg"), color.name());
    emit highlightColorsChanged();
}

bool SettingsStore::continuationMerge() const
{
    return m_settings->value(QStringLiteral("general/continuationMerge"), true).toBool();
}

int SettingsStore::maxLinesPerFile() const
{
    const int value =
        m_settings->value(QStringLiteral("general/maxLinesPerFile"), kDefaultMaxLinesPerFile).toInt();
    return value > 0 ? value : kDefaultMaxLinesPerFile;
}

int SettingsStore::rowHeightLines() const
{
    const int value = m_settings->value(QStringLiteral("view/rowHeightLines"), 2).toInt();
    return qBound(1, value, 10);
}

bool SettingsStore::monitorHighlightNew() const
{
    return m_settings->value(QStringLiteral("general/monitorHighlightNew"), true).toBool();
}

QStringList SettingsStore::recentFiles() const
{
    return m_settings->value(QStringLiteral("files/recent")).toStringList();
}

void SettingsStore::addRecentFile(const QString &path)
{
    if (path.isEmpty())
        return;
    QStringList files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles)
        files.removeLast();
    m_settings->setValue(QStringLiteral("files/recent"), files);
}

void SettingsStore::clearRecentFiles()
{
    m_settings->remove(QStringLiteral("files/recent"));
}

QByteArray SettingsStore::windowGeometry() const
{
    return m_settings->value(QStringLiteral("window/geometry")).toByteArray();
}

void SettingsStore::setWindowGeometry(const QByteArray &state)
{
    m_settings->setValue(QStringLiteral("window/geometry"), state);
}

QByteArray SettingsStore::windowState() const
{
    return m_settings->value(QStringLiteral("window/state")).toByteArray();
}

void SettingsStore::setWindowState(const QByteArray &state)
{
    m_settings->setValue(QStringLiteral("window/state"), state);
}

QByteArray SettingsStore::splitterState() const
{
    return m_settings->value(QStringLiteral("layout/splitterState")).toByteArray();
}

void SettingsStore::setSplitterState(const QByteArray &state)
{
    m_settings->setValue(QStringLiteral("layout/splitterState"), state);
}

QVariantMap SettingsStore::columnWidths(const QString &signature) const
{
    if (signature.isEmpty())
        return {};
    const QVariantMap all = m_settings->value(QStringLiteral("view/columnWidths")).toMap();
    return all.value(signature).toMap();
}

void SettingsStore::setColumnWidths(const QString &signature, const QVariantMap &widths)
{
    if (signature.isEmpty() || widths.isEmpty())
        return;
    QVariantMap all = m_settings->value(QStringLiteral("view/columnWidths")).toMap();
    all.insert(signature, widths);
    m_settings->setValue(QStringLiteral("view/columnWidths"), all);
}

QStringList SettingsStore::hiddenColumns(const QString &signature) const
{
    if (signature.isEmpty())
        return {};
    const QVariantMap all = m_settings->value(QStringLiteral("view/hiddenColumns")).toMap();
    return all.value(signature).toStringList();
}

void SettingsStore::setHiddenColumns(const QString &signature, const QStringList &keys)
{
    if (signature.isEmpty())
        return;
    QVariantMap all = m_settings->value(QStringLiteral("view/hiddenColumns")).toMap();
    if (keys.isEmpty())
        all.remove(signature);      // everything visible again: drop the entry
    else
        all.insert(signature, keys);
    m_settings->setValue(QStringLiteral("view/hiddenColumns"), all);
}

void SettingsStore::resetAll()
{
    m_settings->clear();
    m_settings->sync();
    emit fontsChanged();
    emit themeChanged();
    emit detailsPositionChanged();
    emit detailsBehaviorChanged();
    emit highlightColorsChanged();
}

QString SettingsStore::themeToString(Theme theme)
{
    switch (theme) {
    case Theme::Dark:
        return QLatin1String(kThemeDark);
    case Theme::System:
        return QLatin1String(kThemeSystem);
    case Theme::Light:
        break;
    }
    return QLatin1String(kThemeLight);
}

SettingsStore::Theme SettingsStore::themeFromString(const QString &value)
{
    if (value == QLatin1String(kThemeDark))
        return Theme::Dark;
    if (value == QLatin1String(kThemeSystem))
        return Theme::System;
    return Theme::Light;
}

} // namespace lv
