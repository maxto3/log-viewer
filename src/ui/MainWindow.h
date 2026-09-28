#pragma once

#include "app/SettingsStore.h"
#include "core/IEntryProvider.h"

#include <QMainWindow>
#include <QVector>

#include <memory>

class QAction;
class QActionGroup;
class QGroupBox;
class QLabel;
class QMenu;
class QPushButton;
class QSplitter;
class QStackedWidget;

namespace lv {

class DetailPane;
class FilterPanel;
class HighlightTheme;
class LogSource;
class LogTableModel;
class LogTableView;
class LogWatcher;
class ThemeManager;
class TranslationManager;

/// Application main window: "Search & Filter" group box on top, the log table
/// below it and the detail pane on the right or at the bottom.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(SettingsStore *settings, ThemeManager *theme, TranslationManager *translations,
               QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Loads the synthetic document used by --demo and by UI review sessions.
    void loadDemoData();
    /// Opens the given files (only the first one in this milestone).
    void openPaths(const QStringList &paths, const QString &forcedFormatId = QString());

    // Diagnostics and test accessors ---------------------------------------
    LogTableView *logTableView() const { return m_tableView; }
    DetailPane *detailPane() const { return m_detailPane; }
    LogTableModel *logModel() const { return m_model; }
    bool isDetailPaneVisible() const;
    bool isFullContentMode() const;
    /// Current row of the log table, -1 when nothing is selected.
    int currentRow() const;
    /// File ▸ Monitor action (checkable, enabled for file backed documents).
    QAction *monitorAction() const { return m_monitorAction; }

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    /// Drag and drop of log files onto the window (spec.md REQ-FILE-02).
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onOpen();
    void onRefresh();
    void onCloseDocument();
    void onAbout();
    void onCurrentRowChanged(const QModelIndex &current);
    /// Row activated by the user (click or keyboard navigation).
    void onRowActivatedByUser(int row);
    void onCopyFeedback(const QString &message);
    /// Find / Filter engines (spec.md REQ-FIND / REQ-FILTER).
    void applyFindSettings();
    void applyFilterSettings();
    void gotoMatch(int direction);
    void updateFindCounter();
    void onExportFiltered();
    void applyHighlightColors();
    void applySnippetTheme();
    void chooseFont(int role);
    void chooseHighlightColor();

public slots:
    /// Live monitoring (File ▸ Monitor): appends rows and follows the tail.
    void onRowsAppended(int firstRow, int count);
    void onDocumentRebuilt();
    void onMonitorToggled(bool enabled);

private:
    enum FontRole { InterfaceFontRole = 0, TableFontRole = 1, HeaderFontRole = 2 };

    void createActions();
    void createMenus();
    void createCentralWidget();
    void createStatusBar();
    void connectSignals();
    void restoreWindowGeometry();

    void retranslateUi();
    void applyFonts();
    void applyTheme(bool dark);
    void applyDetailsPosition();
    /// Applies the Settings ▸ Details Pane switch (always visible vs. opened by
    /// clicking a row).
    void applyDetailsBehavior();
    /// Selects the first entry and opens the pane when "always show" is enabled.
    void selectFirstRowIfRequested();
    /// Fills the details pane with \a row and makes sure it is visible.
    void showEntryInDetailPane(int row);
    void startMonitor();
    void stopMonitor();
    void updateMonitorLabel();
    /// VSCode palette for snippets, honouring appearance/syntaxTheme.
    const HighlightTheme &currentHighlightTheme() const;
    /// True when the Find box contains a pattern that was not applied yet.
    bool findPatternIsPending() const;

    void setProvider(const EntryProviderPtr &provider, const QString &path);
    /// Rebuilds the Columns menu from the current document (REQ-UI-12): one
    /// checkable item per column, plus "Show All Columns".
    void rebuildColumnsMenu();
    /// Re-syncs the check states and the "Show All Columns" enablement after a
    /// column was switched on or off.
    void syncColumnsMenu();
    /// Persists column widths + column visibility for the current document
    /// signature (REQ-TABLE-05 / REQ-TABLE-11).
    void saveColumnLayout();
    /// Refreshes the status bar, actions and level counters.
    /// \a documentLoaded also re-syncs the level check boxes with the document
    /// (only on load; a filter change must not touch the user's selection).
    void updateDocumentUi(bool documentLoaded = false);
    void updateRecentFilesMenu();
    void reportError(const QString &message);
    void showDetailPane();

public:
    /// Writes the geometry of the main widgets to \a path; used by the layout
    /// verification script (enabled with the LOGVIEWER_DUMP_LAYOUT env var).
    void dumpLayout(const QString &path);

private:
    void writeLayout(QTextStream &out) const;

    SettingsStore *m_settings = nullptr;
    ThemeManager *m_theme = nullptr;
    TranslationManager *m_translations = nullptr;

    EntryProviderPtr m_provider;
    std::shared_ptr<LogSource> m_source;     ///< file backed provider, if any
    std::unique_ptr<LogWatcher> m_watcher;
    QString m_currentPath;
    QString m_documentSignature;             ///< column layout key (REQ-TABLE-05)
    QString m_forcedFormatId;
    QString m_lastError;

    // Widgets -------------------------------------------------------------
    QWidget *m_content = nullptr;
    FilterPanel *m_filterPanel = nullptr;
    QSplitter *m_splitter = nullptr;
    QGroupBox *m_logGroup = nullptr;
    QStackedWidget *m_logStack = nullptr;
    QWidget *m_emptyState = nullptr;
    QLabel *m_emptyTitle = nullptr;
    QLabel *m_emptyHint = nullptr;
    QPushButton *m_emptyOpenButton = nullptr;
    LogTableView *m_tableView = nullptr;
    LogTableModel *m_model = nullptr;
    DetailPane *m_detailPane = nullptr;

    QLabel *m_docLabel = nullptr;
    QLabel *m_statsLabel = nullptr;
    QLabel *m_monitorLabel = nullptr;

    // Menus and actions ----------------------------------------------------
    QMenu *m_fileMenu = nullptr;
    QMenu *m_settingsMenu = nullptr;
    QMenu *m_columnsMenu = nullptr;
    /// One action per model column, parallel to the current column list.
    QVector<QAction *> m_columnActions;
    QAction *m_showAllColumnsAction = nullptr;
    QMenu *m_recentMenu = nullptr;
    QMenu *m_fontMenu = nullptr;
    QMenu *m_languageMenu = nullptr;
    QMenu *m_layoutMenu = nullptr;
    QMenu *m_detailsMenu = nullptr;
    QMenu *m_appearanceMenu = nullptr;
    QMenu *m_syntaxMenu = nullptr;
    QMenu *m_themeMenu = nullptr;

    QAction *m_openAction = nullptr;
    QAction *m_refreshAction = nullptr;
    QAction *m_closeAction = nullptr;
    QAction *m_monitorAction = nullptr;
    QAction *m_exportAction = nullptr;
    QAction *m_exitAction = nullptr;
    QAction *m_clearRecentAction = nullptr;

    QAction *m_interfaceFontAction = nullptr;
    QAction *m_tableFontAction = nullptr;
    QAction *m_headerFontAction = nullptr;
    QAction *m_resetFontsAction = nullptr;

    QAction *m_englishAction = nullptr;
    QAction *m_chineseAction = nullptr;
    QActionGroup *m_languageGroup = nullptr;

    QAction *m_detailsRightAction = nullptr;
    QAction *m_detailsBottomAction = nullptr;
    QActionGroup *m_layoutGroup = nullptr;

    QAction *m_alwaysShowDetailsAction = nullptr;

    QAction *m_lightThemeAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QAction *m_systemThemeAction = nullptr;
    QActionGroup *m_themeGroup = nullptr;

    QAction *m_highlightColorAction = nullptr;
    QAction *m_syntaxFollowAction = nullptr;
    QAction *m_syntaxDarkAction = nullptr;
    QAction *m_syntaxLightAction = nullptr;
    QActionGroup *m_syntaxGroup = nullptr;
    QAction *m_resetAllAction = nullptr;
    QAction *m_aboutAction = nullptr;

    /// 1-based ordinal of the current Find match (0 = none yet).
    int m_findOrdinal = 0;
};

} // namespace lv
