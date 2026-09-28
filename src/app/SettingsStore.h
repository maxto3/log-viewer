#pragma once

#include <QByteArray>
#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QSettings;

namespace lv {

/// Typed access to settings.ini (see design-doc §8).
///
/// The storage path is resolved from PlatformInfo when no explicit path is
/// given; tests pass a temporary file.
class SettingsStore : public QObject
{
    Q_OBJECT

public:
    enum class Theme { Light, Dark, System };
    enum class DetailsPosition { Right, Bottom };

    explicit SettingsStore(const QString &filePath = QString(), QObject *parent = nullptr);
    ~SettingsStore() override;

    QString filePath() const;

    QString language() const;
    void setLanguage(const QString &code);

    QFont interfaceFont() const;
    void setInterfaceFont(const QFont &font);
    QFont tableFont() const;
    void setTableFont(const QFont &font);
    QFont headerFont() const;
    void setHeaderFont(const QFont &font);
    void resetFonts();

    Theme theme() const;
    void setTheme(Theme theme);
    /// Embedded snippet palette: "follow" (default), "dark+" or "light+".
    QString syntaxTheme() const;
    void setSyntaxTheme(const QString &themeId);
    DetailsPosition detailsPosition() const;
    void setDetailsPosition(DetailsPosition position);

    /// Details pane switch (Settings ▸ Details Pane ▸ "Show Details Pane"):
    ///  - enabled  -> the pane is always visible; loading a document selects the
    ///                first entry and clicking a row updates the pane;
    ///  - disabled -> the pane is never shown (clicking a row does nothing).
    bool showDetailsPane() const;
    void setShowDetailsPane(bool enabled);

    QColor highlightBackground() const;
    void setHighlightBackground(const QColor &color);
    QColor highlightForeground() const;
    void setHighlightForeground(const QColor &color);

    bool continuationMerge() const;
    int maxLinesPerFile() const;
    int rowHeightLines() const;
    bool monitorHighlightNew() const;

    QStringList recentFiles() const;
    void addRecentFile(const QString &path);
    void clearRecentFiles();

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &state);
    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);
    QByteArray splitterState() const;
    void setSplitterState(const QByteArray &state);

    /// Column widths remembered per document signature (format + column titles,
    /// see REQ-TABLE-05). The map is column title -> pixel width.
    QVariantMap columnWidths(const QString &signature) const;
    void setColumnWidths(const QString &signature, const QVariantMap &widths);

    /// Hidden columns (keys, see LogTableModel::columnKey) remembered per document
    /// signature, like the widths (REQ-TABLE-11). An empty list means "all
    /// visible".
    QStringList hiddenColumns(const QString &signature) const;
    void setHiddenColumns(const QString &signature, const QStringList &keys);

    /// Restores every value to its default.
    void resetAll();

signals:
    void fontsChanged();
    void themeChanged();
    void detailsPositionChanged();
    void detailsBehaviorChanged();
    void highlightColorsChanged();

private:
    static QString themeToString(Theme theme);
    static Theme themeFromString(const QString &value);

    /// Migrates settings written by earlier versions (see design-doc §8).
    void migrateLegacyKeys();

    std::unique_ptr<QSettings> m_settings;
};

} // namespace lv
