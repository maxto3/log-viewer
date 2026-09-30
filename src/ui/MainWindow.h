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
class QShortcut;
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
class StatusWarningLabel;
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
    FilterPanel *filterPanel() const { return m_filterPanel; }
    LogTableModel *logModel() const { return m_model; }
    bool isDetailPaneVisible() const;
    bool isFullContentMode() const;
    /// Current row of the log table, -1 when nothing is selected.
    int currentRow() const;
    /// File ▸ Monitor action (checkable, enabled for file backed documents).
    QAction *monitorAction() const { return m_monitorAction; }
    /// Settings ▸ Full Screen action (checkable, F11, REQ-UI-14).
    QAction *fullScreenAction() const { return m_fullScreenAction; }
    /// Status bar label with the duration of the last file open (REQ-UI-15).
    QLabel *loadTimeLabel() const { return m_loadTimeLabel; }
    /// Status bar warning indicator: persistent failure alerts (REQ-REL-01).
    QLabel *warningLabel() const;
    /// Status bar "Open as Administrator…" offer for permission failures
    /// (REQ-REL-04); null only before the status bar is created.
    QPushButton *elevateButton() const { return m_elevateButton; }
    /// Private snapshot backing the current elevated document (empty = regular).
    QString elevatedSnapshotPath() const { return m_snapshotPath; }
    /// Settings ▸ File Association action (checkable, REQ-UI-16).
    QAction *associateAction() const { return m_associateAction; }

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
    /// Settings ▸ File Association (REQ-UI-16): registers or removes the .log
    /// association for the current user, then re-reads the real state.
    void onToggleAssociation(bool checked);
    void onCurrentRowChanged(const QModelIndex &current);
    /// Row activated by the user (click or keyboard navigation).
    void onRowActivatedByUser(int row);
    void onCopyFeedback(const QString &message);
    /// "Open as Administrator…" in the status bar (REQ-REL-04).
    void onElevateClicked();
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
    /// Settings ▸ Full Screen (F11): enters full screen with the search &
    /// filter panel collapsed, or leaves it and restores the panel state the
    /// user had before (REQ-UI-14).
    void setFullScreen(bool fullScreen);

private:
    enum FontRole { InterfaceFontRole = 0, TableFontRole = 1, HeaderFontRole = 2 };

    void createActions();
    void createMenus();
    void createCentralWidget();
    void createStatusBar();
    void connectSignals();
    void restoreWindowGeometry();

    void retranslateUi();
    /// Updates the empty state hint: the failure message in the warning colour
    /// while an error is active (REQ-REL-01), the normal hint otherwise.
    void updateEmptyHint();
    void applyFonts();
    void applyTheme(bool dark);
    /// Reacts to window state changes (menu check mark, Esc shortcut and the
    /// automatic collapse of the search & filter panel in full screen).
    void applyFullScreenState();
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
    /// Shared open implementation behind openPaths() and openElevated(): the
    /// latter passes \a keepSnapshot = true because the private snapshot was
    /// registered just before. Returns whether a document was attached.
    bool openPathsInternal(const QStringList &paths, const QString &forcedFormatId,
                           const QStringList &recentPaths, QString currentPath,
                           bool keepSnapshot);
    /// Opens \a path through the system authentication helper as a read-only
    /// snapshot (REQ-REL-04); failures keep the red warning and the offer.
    void openElevated(const QString &path);
    /// Shows the "Open as Administrator…" offer when one of \a paths is a
    /// regular file the current user cannot read.
    void updateElevationOffer(const QStringList &paths);
    /// Hides the offer and drops the pending candidate.
    void clearElevationOffer();
    /// Deletes the snapshot of the current elevated document (if any).
    void releaseSnapshot();
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
    /// Re-reads the .log association from the registry so the menu entry shows
    /// whether the association currently points at this executable.
    void syncAssociationAction();
    void reportError(const QString &message);
    void showDetailPane();
    /// Stores the duration of the last document open and refreshes the status
    /// bar label (REQ-UI-15); a negative value clears it.
    void setLoadTime(qint64 milliseconds);
    void updateLoadTimeLabel();

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
    /// Candidate file for the elevated open offered by the status bar button.
    QString m_elevationCandidate;
    /// Private read-only snapshot backing the current elevated document.
    QString m_snapshotPath;
    /// Original (unreadable) path of the elevated document; Refresh re-reads it
    /// with fresh authorization.
    QString m_elevatedOriginalPath;

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
    /// Bottom-left status bar warning indicator (REQ-REL-01): persistent red
    /// failure alert. Normal widget, so temporary messages cover it briefly.
    StatusWarningLabel *m_warningLabel = nullptr;
    /// "Open as Administrator…" next to the warning (REQ-REL-04).
    QPushButton *m_elevateButton = nullptr;
    /// Bottom-left status bar label: duration of the last document open
    /// (REQ-UI-15). Normal indicator, so a temporary message hides it briefly.
    QLabel *m_loadTimeLabel = nullptr;
    /// Milliseconds the last document open took; negative = nothing to show.
    qint64 m_lastLoadMs = -1;

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
    /// Settings ▸ File Association (REQ-UI-16).
    QMenu *m_fileAssociationMenu = nullptr;

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

    QAction *m_fullScreenAction = nullptr;
    /// Esc leaves full screen; disabled while the window is not full screen so
    /// the key stays available for other widgets (REQ-UI-14).
    QShortcut *m_escapeShortcut = nullptr;
    /// True while the window is in full screen mode.
    bool m_fullScreenActive = false;
    /// Collapse state of the search & filter panel before full screen, restored
    /// when the user leaves it again.
    bool m_collapsedBeforeFullScreen = false;
    /// Maximized state before full screen: leaving full screen returns to the
    /// maximized window instead of the restored one.
    bool m_wasMaximizedBeforeFullScreen = false;
    /// Window geometry before full screen; saved instead of the full screen
    /// geometry when the window is closed while full screen (REQ-UI-14).
    QByteArray m_geometryBeforeFullScreen;

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
    /// Checkable "Associate .log Files" entry (REQ-UI-16).
    QAction *m_associateAction = nullptr;

    /// 1-based ordinal of the current Find match (0 = none yet).
    int m_findOrdinal = 0;
};

} // namespace lv
