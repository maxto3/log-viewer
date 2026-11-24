#include "ui/MainWindow.h"

#include "app/ThemeManager.h"
#include "app/TranslationManager.h"
#include "core/DurationFormat.h"
#include "core/EventLogProvider.h"
#include "core/EventLogWatcher.h"
#include "core/FilterSpec.h"
#include "core/LogDocument.h"
#include "core/LogSource.h"
#include "core/LogTableModel.h"
#include "core/LogWatcher.h"
#include "core/Matcher.h"
#include "highlight/HighlightTheme.h"
#include "platform/ElevatedEventLog.h"
#include "platform/ElevatedFileReader.h"
#include "platform/FileAssociation.h"
#include "ui/AboutDialog.h"
#include "ui/DemoData.h"
#include "ui/DetailPane.h"
#include "ui/EventLogChannelDialog.h"
#include "ui/FilterPanel.h"
#include "ui/LogTableView.h"
#include "ui/StatusWarningLabel.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTextStream>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

namespace lv {
namespace {

constexpr int kContentMargin = 16;
constexpr int kGroupSpacing = 12;

QFont boldFont(QFont font)
{
    font.setBold(true);
    return font;
}

} // namespace

MainWindow::MainWindow(SettingsStore *settings, ThemeManager *theme, TranslationManager *translations,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_translations(translations)
{
    setMinimumSize(1024, 600);

    // Accept log files dropped onto the window (REQ-FILE-02). Child widgets that
    // otherwise handle drops (line edits, the message view) opt out so that a
    // drop anywhere in the window opens the file.
    setAcceptDrops(true);

    createActions();
    m_eventLogBackend = createEventLogBackend();
    createMenus();
    createCentralWidget();
    createStatusBar();
    connectSignals();
    restoreWindowGeometry();

    applyTheme(m_theme->isDark());
    applyFonts();
    applyHighlightColors();
    applyDetailsPosition();
    applyDetailsBehavior();
    updateRecentFilesMenu();
    updateDocumentUi();
    retranslateUi();
}

void MainWindow::restoreWindowGeometry()
{
    QScreen *targetScreen = screen() ? screen() : QApplication::primaryScreen();
    const QRect available = targetScreen ? targetScreen->availableGeometry() : QRect(0, 0, 1920, 1080);

    // Never require more space than the screen provides (REQ-PLAT-06).
    const int minWidth = qMin(1024, available.width());
    const int minHeight = qMin(600, available.height());
    setMinimumSize(minWidth, minHeight);

    const int defaultWidth = qBound(minWidth, 1600, qMax(minWidth, static_cast<int>(available.width() * 0.92)));
    const int defaultHeight = qBound(minHeight, 950, qMax(minHeight, static_cast<int>(available.height() * 0.92)));
    resize(defaultWidth, defaultHeight);
    move(available.center() - QPoint(defaultWidth / 2, defaultHeight / 2));

    const QByteArray saved = m_settings->windowGeometry();
    if (!saved.isEmpty())
        restoreGeometry(saved);

    // A saved geometry may come from a different screen or DPI setting: clamp it.
    QRect frame = frameGeometry();
    int width = qMin(frame.width(), available.width());
    int height = qMin(frame.height(), available.height());
    if (width != frame.width() || height != frame.height())
        resize(width, height);

    frame = frameGeometry();
    if (!available.contains(frame.center())) {
        const QPoint topLeft = available.center() - QPoint(frame.width() / 2, frame.height() / 2);
        move(qBound(available.left(), topLeft.x(), available.right() - frame.width()),
             qBound(available.top(), topLeft.y(), available.bottom() - frame.height()));
    }
}

MainWindow::~MainWindow()
{
    releaseSnapshot();
}

void MainWindow::createActions()
{
    m_openAction = new QAction(this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpen);

    m_refreshAction = new QAction(this);
    m_refreshAction->setShortcut(QKeySequence::Refresh);
    connect(m_refreshAction, &QAction::triggered, this, &MainWindow::onRefresh);

    m_closeAction = new QAction(this);
    m_closeAction->setShortcut(QKeySequence::Close);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::onCloseDocument);

    m_monitorAction = new QAction(this);
    m_monitorAction->setCheckable(true);
    m_monitorAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    m_monitorAction->setEnabled(false);   // enabled for file backed documents
    connect(m_monitorAction, &QAction::toggled, this, &MainWindow::onMonitorToggled);

    m_exportAction = new QAction(this);
    m_exportAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::onExportFiltered);

    // Only meaningful for a truncated native event log; enabled by
    // updateDocumentUi() when such a document is attached (REQ-EVT-09).
    m_loadMoreEventsAction = new QAction(this);
    m_loadMoreEventsAction->setEnabled(false);
    connect(m_loadMoreEventsAction, &QAction::triggered, this, &MainWindow::onLoadMoreEvents);

    m_exitAction = new QAction(this);
    m_exitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::close);

    m_clearRecentAction = new QAction(this);
    connect(m_clearRecentAction, &QAction::triggered, this, [this] {
        m_settings->clearRecentFiles();
        updateRecentFilesMenu();
    });

    m_interfaceFontAction = new QAction(this);
    connect(m_interfaceFontAction, &QAction::triggered, this,
            [this] { chooseFont(InterfaceFontRole); });
    m_tableFontAction = new QAction(this);
    connect(m_tableFontAction, &QAction::triggered, this, [this] { chooseFont(TableFontRole); });
    m_headerFontAction = new QAction(this);
    connect(m_headerFontAction, &QAction::triggered, this, [this] { chooseFont(HeaderFontRole); });
    m_resetFontsAction = new QAction(this);
    connect(m_resetFontsAction, &QAction::triggered, this, [this] { m_settings->resetFonts(); });

    m_languageGroup = new QActionGroup(this);
    m_englishAction = new QAction(m_languageGroup);
    m_englishAction->setCheckable(true);
    m_chineseAction = new QAction(m_languageGroup);
    m_chineseAction->setCheckable(true);
    connect(m_englishAction, &QAction::triggered, this, [this] {
        m_settings->setLanguage(QStringLiteral("en"));
        m_translations->setLanguage(QStringLiteral("en"));
    });
    connect(m_chineseAction, &QAction::triggered, this, [this] {
        m_settings->setLanguage(QStringLiteral("zh_CN"));
        m_translations->setLanguage(QStringLiteral("zh_CN"));
    });

    m_layoutGroup = new QActionGroup(this);
    m_detailsRightAction = new QAction(m_layoutGroup);
    m_detailsRightAction->setCheckable(true);
    m_detailsBottomAction = new QAction(m_layoutGroup);
    m_detailsBottomAction->setCheckable(true);
    connect(m_detailsRightAction, &QAction::triggered, this, [this] {
        m_settings->setDetailsPosition(SettingsStore::DetailsPosition::Right);
    });
    connect(m_detailsBottomAction, &QAction::triggered, this, [this] {
        m_settings->setDetailsPosition(SettingsStore::DetailsPosition::Bottom);
    });

    m_alwaysShowDetailsAction = new QAction(this);
    m_alwaysShowDetailsAction->setCheckable(true);
    connect(m_alwaysShowDetailsAction, &QAction::toggled, this, [this](bool checked) {
        m_settings->setShowDetailsPane(checked);
    });

    m_themeGroup = new QActionGroup(this);
    m_lightThemeAction = new QAction(m_themeGroup);
    m_lightThemeAction->setCheckable(true);
    m_darkThemeAction = new QAction(m_themeGroup);
    m_darkThemeAction->setCheckable(true);
    m_systemThemeAction = new QAction(m_themeGroup);
    m_systemThemeAction->setCheckable(true);
    const auto selectTheme = [this](SettingsStore::Theme mode) {
        m_settings->setTheme(mode);
        m_theme->apply();
    };
    connect(m_lightThemeAction, &QAction::triggered, this,
            [selectTheme] { selectTheme(SettingsStore::Theme::Light); });
    connect(m_darkThemeAction, &QAction::triggered, this,
            [selectTheme] { selectTheme(SettingsStore::Theme::Dark); });
    connect(m_systemThemeAction, &QAction::triggered, this,
            [selectTheme] { selectTheme(SettingsStore::Theme::System); });

    m_highlightColorAction = new QAction(this);
    connect(m_highlightColorAction, &QAction::triggered, this,
            &MainWindow::chooseHighlightColor);

    m_syntaxGroup = new QActionGroup(this);
    m_syntaxFollowAction = new QAction(m_syntaxGroup);
    m_syntaxFollowAction->setCheckable(true);
    m_syntaxDarkAction = new QAction(m_syntaxGroup);
    m_syntaxDarkAction->setCheckable(true);
    m_syntaxLightAction = new QAction(m_syntaxGroup);
    m_syntaxLightAction->setCheckable(true);
    const auto selectSyntax = [this](const QString &id) {
        m_settings->setSyntaxTheme(id);
        applySnippetTheme();
    };
    connect(m_syntaxFollowAction, &QAction::triggered, this,
            [selectSyntax] { selectSyntax(QStringLiteral("follow")); });
    connect(m_syntaxDarkAction, &QAction::triggered, this,
            [selectSyntax] { selectSyntax(QStringLiteral("dark+")); });
    connect(m_syntaxLightAction, &QAction::triggered, this,
            [selectSyntax] { selectSyntax(QStringLiteral("light+")); });

    m_fullScreenAction = new QAction(this);
    m_fullScreenAction->setCheckable(true);
    m_fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(m_fullScreenAction, &QAction::triggered, this, &MainWindow::setFullScreen);

    // Esc only leaves full screen while the window actually is in full screen
    // mode, so the key stays free for the table and the dialogs otherwise
    // (REQ-UI-14).
    m_escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_escapeShortcut->setContext(Qt::WindowShortcut);
    m_escapeShortcut->setEnabled(false);
    connect(m_escapeShortcut, &QShortcut::activated, this, [this] { setFullScreen(false); });

    m_resetAllAction = new QAction(this);
    connect(m_resetAllAction, &QAction::triggered, this, [this] {
        const auto answer = QMessageBox::question(
            this, tr("Reset All Settings"),
            tr("Restore every setting (fonts, language, layout, appearance) to its default value?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        m_settings->resetAll();
        m_translations->setLanguage(m_settings->language());
        m_theme->apply();
        updateRecentFilesMenu();
        updateDocumentUi();
    });

    m_aboutAction = new QAction(this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    // Settings ▸ File Association (REQ-UI-16): the check mark is only a mirror
    // of the registry, it is refreshed from the registry after every change.
    m_associateAction = new QAction(this);
    m_associateAction->setCheckable(true);
    m_associateAction->setEnabled(FileAssociation::isSupported());
    connect(m_associateAction, &QAction::triggered, this, &MainWindow::onToggleAssociation);
}

void MainWindow::createMenus()
{
    m_fileMenu = menuBar()->addMenu(QString());
    m_fileMenu->addAction(m_openAction);
    // Windows only: open a live channel or an archived .evtx file (REQ-EVT-01).
    buildEventLogMenu();
    if (m_eventLogBackend && m_eventLogBackend->isSupported())
        m_fileMenu->addAction(m_loadMoreEventsAction);
    m_fileMenu->addAction(m_refreshAction);
    m_fileMenu->addAction(m_closeAction);
    m_fileMenu->addAction(m_monitorAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exportAction);
    m_fileMenu->addSeparator();
    m_recentMenu = m_fileMenu->addMenu(QString());
    m_recentMenu->addAction(m_clearRecentAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    m_settingsMenu = menuBar()->addMenu(QString());
    m_fontMenu = m_settingsMenu->addMenu(QString());
    m_fontMenu->addAction(m_interfaceFontAction);
    m_fontMenu->addAction(m_tableFontAction);
    m_fontMenu->addAction(m_headerFontAction);
    m_fontMenu->addSeparator();
    m_fontMenu->addAction(m_resetFontsAction);

    m_languageMenu = m_settingsMenu->addMenu(QString());
    m_languageMenu->addAction(m_englishAction);
    m_languageMenu->addAction(m_chineseAction);

    m_detailsMenu = m_settingsMenu->addMenu(QString());
    m_detailsMenu->addAction(m_alwaysShowDetailsAction);
    // Layout lives inside the details pane menu: it only affects that pane
    // (spec.md REQ-UI-04).
    m_layoutMenu = m_detailsMenu->addMenu(QString());
    m_layoutMenu->addAction(m_detailsRightAction);
    m_layoutMenu->addAction(m_detailsBottomAction);

    m_appearanceMenu = m_settingsMenu->addMenu(QString());
    m_themeMenu = m_appearanceMenu->addMenu(QString());
    m_themeMenu->addAction(m_lightThemeAction);
    m_themeMenu->addAction(m_darkThemeAction);
    m_themeMenu->addAction(m_systemThemeAction);
    m_appearanceMenu->addAction(m_highlightColorAction);
    m_syntaxMenu = m_appearanceMenu->addMenu(QString());
    m_syntaxMenu->addAction(m_syntaxFollowAction);
    m_syntaxMenu->addAction(m_syntaxDarkAction);
    m_syntaxMenu->addAction(m_syntaxLightAction);
    m_appearanceMenu->addSeparator();
    m_appearanceMenu->addAction(m_resetAllAction);

    // Per-user file association (REQ-UI-16). The check mark mirrors the
    // registry, so it is re-read every time the submenu is opened.
    m_fileAssociationMenu = m_settingsMenu->addMenu(QString());
    m_fileAssociationMenu->addAction(m_associateAction);
    connect(m_fileAssociationMenu, &QMenu::aboutToShow, this, &MainWindow::syncAssociationAction);

    // Full screen is a view mode, but it lives in the Settings menu
    // (REQ-UI-14) next to the other window level switches.
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_fullScreenAction);

    // Columns is a top level menu between Settings and About (REQ-UI-12); its
    // items are rebuilt from the document that is loaded (REQ-TABLE-11).
    m_columnsMenu = menuBar()->addMenu(QString());
    m_columnsMenu->setEnabled(false);

    // About is a top level menu bar entry, next to File and Settings
    // (spec.md REQ-UI-06); it opens the dialog directly.
    menuBar()->addAction(m_aboutAction);
}

void MainWindow::buildEventLogMenu()
{
    if (!m_eventLogBackend || !m_eventLogBackend->isSupported())
        return;

    m_eventLogMenu = m_fileMenu->addMenu(QString());

    // Classic channels exist on every Windows installation (REQ-EVT-02). The
    // names are technical identifiers from the Windows API and are not
    // translated.
    const QStringList commonChannels = {
        QStringLiteral("Application"), QStringLiteral("System"), QStringLiteral("Setup"),
        QStringLiteral("Security")};
    for (const QString &channel : commonChannels) {
        QAction *action = m_eventLogMenu->addAction(channel);
        connect(action, &QAction::triggered, this,
                [this, channel] { openEventLogChannel(channel); });
    }

    m_eventLogMenu->addSeparator();
    m_eventLogAllChannelsAction = m_eventLogMenu->addAction(QString());
    connect(m_eventLogAllChannelsAction, &QAction::triggered, this,
            &MainWindow::onOpenEventLogChannels);

    m_eventLogFileAction = m_eventLogMenu->addAction(QString());
    connect(m_eventLogFileAction, &QAction::triggered, this, &MainWindow::onOpenEventLogFile);
}

void MainWindow::createCentralWidget()
{
    auto *central = new QWidget(this);
    auto *outer = new QHBoxLayout(central);
    // Symmetric margins; the content column stretches with the window so a
    // maximised window is used down to the last pixel (spec.md REQ-VIS-02).
    outer->setContentsMargins(kContentMargin, kContentMargin / 2 + 6, kContentMargin,
                              kContentMargin / 2 + 6);
    outer->setSpacing(0);

    m_content = new QWidget(central);
    outer->addWidget(m_content);

    auto *contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(kGroupSpacing);

    m_filterPanel = new FilterPanel(m_content);
    contentLayout->addWidget(m_filterPanel);

    m_splitter = new QSplitter(Qt::Horizontal, m_content);
    m_splitter->setChildrenCollapsible(false);

    m_logGroup = new QGroupBox(m_splitter);
    auto *logLayout = new QVBoxLayout(m_logGroup);
    logLayout->setContentsMargins(8, 14, 8, 8);

    m_logStack = new QStackedWidget(m_logGroup);
    logLayout->addWidget(m_logStack);

    // Empty state page.
    m_emptyState = new QWidget(m_logStack);
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->addStretch(1);
    m_emptyTitle = new QLabel(m_emptyState);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyTitle->setFont(boldFont(m_emptyTitle->font()));
    emptyLayout->addWidget(m_emptyTitle);
    m_emptyHint = new QLabel(m_emptyState);
    m_emptyHint->setObjectName(QStringLiteral("emptyHint"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    emptyLayout->addWidget(m_emptyHint);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    m_emptyOpenButton = new QPushButton(m_emptyState);
    connect(m_emptyOpenButton, &QPushButton::clicked, this, &MainWindow::onOpen);
    buttonRow->addWidget(m_emptyOpenButton);

    // Second empty state entry on Windows (REQ-EVT-01); hidden elsewhere.
    m_emptyEventLogButton = new QPushButton(m_emptyState);
    m_emptyEventLogButton->setVisible(m_eventLogBackend && m_eventLogBackend->isSupported());
    connect(m_emptyEventLogButton, &QPushButton::clicked, this,
            &MainWindow::onOpenEventLogChannels);
    buttonRow->addWidget(m_emptyEventLogButton);
    buttonRow->addStretch(1);
    emptyLayout->addLayout(buttonRow);
    emptyLayout->addStretch(1);

    // Table page.
    m_model = new LogTableModel(this);
    m_tableView = new LogTableView(m_logStack);
    m_tableView->setLogModel(m_model);
    m_tableView->setRowHeightLines(m_settings->rowHeightLines());

    m_logStack->addWidget(m_emptyState);
    m_logStack->addWidget(m_tableView);

    m_detailPane = new DetailPane(m_splitter);
    m_detailPane->hide();

    m_splitter->addWidget(m_logGroup);
    m_splitter->addWidget(m_detailPane);

    contentLayout->addWidget(m_splitter, 1);
    setCentralWidget(central);
}

void MainWindow::createStatusBar()
{
    // Left-most indicator: persistent failure warnings (REQ-REL-01). It stays
    // hidden until the first error and never disappears on a timeout.
    m_warningLabel = new StatusWarningLabel(statusBar());
    statusBar()->addWidget(m_warningLabel);

    // Optional follow-up action for permission failures (REQ-REL-04): opens the
    // file through the system authentication helper as a read-only snapshot.
    // Filled warning colour (theme style sheet) so it stands out in the bar.
    m_elevateButton = new QPushButton(tr("Open as Administrator…"), statusBar());
    m_elevateButton->setObjectName(QStringLiteral("elevateButton"));
    m_elevateButton->setCursor(Qt::PointingHandCursor);
    m_elevateButton->hide();
    connect(m_elevateButton, &QPushButton::clicked, this, &MainWindow::onElevateClicked);
    statusBar()->addWidget(m_elevateButton);

    // Windows event logs: appears only while the current document is truncated
    // (REQ-EVT-09). A permanent widget so a temporary status message (e.g. the
    // truncation notice) does not hide it.
    m_loadMoreEventsButton = new QPushButton(tr("Load More Events…"), statusBar());
    m_loadMoreEventsButton->setObjectName(QStringLiteral("loadMoreEventsButton"));
    m_loadMoreEventsButton->setCursor(Qt::PointingHandCursor);
    m_loadMoreEventsButton->hide();
    connect(m_loadMoreEventsButton, &QPushButton::clicked, this, &MainWindow::onLoadMoreEvents);
    statusBar()->addPermanentWidget(m_loadMoreEventsButton);

    // Bottom left: duration of the last file open (REQ-UI-15). Normal status
    // widgets sit on the left and give way to temporary messages.
    m_loadTimeLabel = new QLabel(statusBar());
    m_loadTimeLabel->setObjectName(QStringLiteral("loadTimeLabel"));
    statusBar()->addWidget(m_loadTimeLabel);

    m_docLabel = new QLabel(statusBar());
    m_docLabel->setObjectName(QStringLiteral("docLabel"));
    m_statsLabel = new QLabel(statusBar());
    m_monitorLabel = new QLabel(statusBar());
    // All labels are permanent (right aligned): the temporary message area on
    // the left stays free for showMessage() texts such as copy feedback.
    statusBar()->addPermanentWidget(m_docLabel);
    statusBar()->addPermanentWidget(m_statsLabel);
    statusBar()->addPermanentWidget(m_monitorLabel);
    statusBar()->showMessage(QString());
}

QLabel *MainWindow::warningLabel() const
{
    return m_warningLabel;
}

void MainWindow::connectSignals()
{
    m_watcher = std::make_unique<LogWatcher>(this);
    connect(m_watcher.get(), &LogWatcher::rowsAppended, this, &MainWindow::onRowsAppended);
    connect(m_watcher.get(), &LogWatcher::documentRebuilt, this, &MainWindow::onDocumentRebuilt);
    connect(m_watcher.get(), &LogWatcher::watchingFailed, this, &MainWindow::reportError);

    connect(m_settings, &SettingsStore::fontsChanged, this, &MainWindow::applyFonts);
    connect(m_settings, &SettingsStore::detailsPositionChanged, this,
            &MainWindow::applyDetailsPosition);
    connect(m_settings, &SettingsStore::detailsBehaviorChanged, this,
            &MainWindow::applyDetailsBehavior);
    connect(m_theme, &ThemeManager::themeChanged, this, &MainWindow::applyTheme);
    connect(m_settings, &SettingsStore::themeChanged, this, [this] {
        m_lightThemeAction->setChecked(m_settings->theme() == SettingsStore::Theme::Light);
        m_darkThemeAction->setChecked(m_settings->theme() == SettingsStore::Theme::Dark);
        m_systemThemeAction->setChecked(m_settings->theme() == SettingsStore::Theme::System);
    });

    connect(m_tableView, &LogTableView::copyFeedback, this, &MainWindow::onCopyFeedback);
    connect(m_tableView, &LogTableView::findNextRequested, this, [this] { gotoMatch(1); });
    connect(m_tableView, &LogTableView::findPreviousRequested, this, [this] { gotoMatch(-1); });

    if (QItemSelectionModel *selection = m_tableView->selectionModel()) {
        connect(selection, &QItemSelectionModel::currentRowChanged, this,
                [this](const QModelIndex &current, const QModelIndex &) {
                    onCurrentRowChanged(current);
                });
    }
    connect(m_tableView, &LogTableView::rowActivatedByUser, this,
            &MainWindow::onRowActivatedByUser);

    connect(m_filterPanel, &FilterPanel::findChanged, this, [this](const QString &, int, bool) {
        applyFindSettings();
    });
    connect(m_filterPanel, &FilterPanel::filterChanged, this, [this](const QString &, int, bool) {
        applyFilterSettings();
    });
    connect(m_filterPanel, &FilterPanel::levelsChanged, this, [this](int) { applyFilterSettings(); });
    connect(m_filterPanel, &FilterPanel::timeRangeChanged, this, &MainWindow::applyFilterSettings);
    connect(m_filterPanel, &FilterPanel::findNextRequested, this, [this] { gotoMatch(1); });
    connect(m_filterPanel, &FilterPanel::findPreviousRequested, this, [this] { gotoMatch(-1); });

    connect(m_settings, &SettingsStore::highlightColorsChanged, this,
            &MainWindow::applyHighlightColors);
}

void MainWindow::retranslateUi()
{
    setWindowTitle(QStringLiteral("Log Viewer"));
    m_fileMenu->setTitle(tr("&File"));
    m_settingsMenu->setTitle(tr("&Settings"));
    m_columnsMenu->setTitle(tr("&Columns"));
    rebuildColumnsMenu();
    m_model->retranslateHeaders();
    m_recentMenu->setTitle(tr("Recent Files"));
    m_fontMenu->setTitle(tr("Font"));
    m_languageMenu->setTitle(tr("Language"));
    m_layoutMenu->setTitle(tr("Layout"));
    m_detailsMenu->setTitle(tr("Details Pane"));
    m_appearanceMenu->setTitle(tr("Appearance"));
    m_themeMenu->setTitle(tr("Theme"));
    m_openAction->setText(tr("Open…"));
    m_refreshAction->setText(tr("Refresh"));
    m_closeAction->setText(tr("Close"));
    m_monitorAction->setText(tr("Monitor"));
    m_monitorAction->setToolTip(
        tr("Follow the file or event log channel like \"tail -f\" (Ctrl+M)"));
    m_exportAction->setText(tr("Export Filtered Results…"));
    m_exitAction->setText(tr("Exit"));
    m_clearRecentAction->setText(tr("Clear List"));

    if (m_eventLogMenu)
        m_eventLogMenu->setTitle(tr("Windows Event Log"));
    if (m_eventLogAllChannelsAction)
        m_eventLogAllChannelsAction->setText(tr("All Channels…"));
    if (m_eventLogFileAction)
        m_eventLogFileAction->setText(tr("Open .evtx File…"));
    m_loadMoreEventsAction->setText(tr("Load More Events…"));
    m_loadMoreEventsAction->setToolTip(
        tr("Load older events from the current Windows event log"));
    m_loadMoreEventsButton->setText(tr("Load More Events…"));
    m_loadMoreEventsButton->setToolTip(m_loadMoreEventsAction->toolTip());

    m_interfaceFontAction->setText(tr("Interface Font…"));
    m_tableFontAction->setText(tr("Log Table Font…"));
    m_headerFontAction->setText(tr("Log Header Font…"));
    m_resetFontsAction->setText(tr("Reset to Defaults"));

    m_englishAction->setText(tr("English"));
    m_chineseAction->setText(QStringLiteral("简体中文"));

    m_detailsRightAction->setText(tr("Details: Right"));
    m_detailsBottomAction->setText(tr("Details: Bottom"));

    m_alwaysShowDetailsAction->setText(tr("Show Details Pane"));
    m_alwaysShowDetailsAction->setToolTip(
        tr("Checked: the details pane is always visible (the first entry is selected when a log is "
           "loaded). Unchecked: the pane is never shown - use Enter to expand a row instead"));

    m_lightThemeAction->setText(tr("Light"));
    m_darkThemeAction->setText(tr("Dark"));
    m_systemThemeAction->setText(tr("Follow System"));
    m_highlightColorAction->setText(tr("Highlight Color…"));
    m_syntaxMenu->setTitle(tr("Syntax Highlighting"));
    m_syntaxFollowAction->setText(tr("Follow Theme"));
    m_syntaxDarkAction->setText(QStringLiteral("VSCode Dark+"));
    m_syntaxLightAction->setText(QStringLiteral("VSCode Light+"));
    m_resetAllAction->setText(tr("Reset All Settings"));

    m_fullScreenAction->setText(tr("Full Screen"));
    m_fullScreenAction->setToolTip(
        tr("Collapse the search and filter panel and use the whole screen (F11; leave with Esc)"));

    m_fileAssociationMenu->setTitle(tr("File Association"));
    m_associateAction->setText(tr("Associate .log Files"));
    m_associateAction->setToolTip(
        FileAssociation::isSupported()
            ? tr("Register .log files for the current user so that double-clicking one opens this "
                 "executable. Uncheck to remove the association again.")
            : FileAssociation::unsupportedText());
    syncAssociationAction();

    m_aboutAction->setText(tr("About"));
    m_logGroup->setTitle(tr("Log"));
    m_emptyTitle->setText(tr("No log file open"));
    updateEmptyHint();
    m_emptyOpenButton->setText(tr("Open Log File…"));
    if (m_emptyEventLogButton)
        m_emptyEventLogButton->setText(tr("Open Windows Event Log…"));
    m_elevateButton->setText(tr("Open as Administrator…"));

    m_monitorLabel->setText(tr("Monitor: off"));
    updateMonitorLabel();
    m_filterPanel->retranslateUi();
    m_detailPane->retranslateUi();

    // Language and theme menu state.
    m_englishAction->setChecked(m_settings->language() != QLatin1String("zh_CN"));
    m_chineseAction->setChecked(m_settings->language() == QLatin1String("zh_CN"));
    m_detailsRightAction->setChecked(m_settings->detailsPosition()
                                     == SettingsStore::DetailsPosition::Right);
    m_detailsBottomAction->setChecked(m_settings->detailsPosition()
                                      == SettingsStore::DetailsPosition::Bottom);
    m_alwaysShowDetailsAction->setChecked(m_settings->showDetailsPane());
    m_lightThemeAction->setChecked(m_settings->theme() == SettingsStore::Theme::Light);
    m_darkThemeAction->setChecked(m_settings->theme() == SettingsStore::Theme::Dark);
    m_systemThemeAction->setChecked(m_settings->theme() == SettingsStore::Theme::System);

    const QString syntax = m_settings->syntaxTheme();
    m_syntaxFollowAction->setChecked(syntax == QLatin1String("follow"));
    m_syntaxDarkAction->setChecked(syntax == QLatin1String("dark+"));
    m_syntaxLightAction->setChecked(syntax == QLatin1String("light+"));

    updateDocumentUi();
    updateLoadTimeLabel();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    int files = 0;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile())
            ++files;
    }
    if (files == 0) {
        event->ignore();
        return;
    }

    event->acceptProposedAction();
    statusBar()->showMessage(tr("Drop to open %n file(s)", "", files), 3000);
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    int folders = 0;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
        const QFileInfo info(url.toLocalFile());
        if (info.isDir()) {
            ++folders;
            continue;
        }
        if (info.isFile())
            paths << info.absoluteFilePath();
    }

    event->acceptProposedAction();

    if (folders > 0) {
        statusBar()->showMessage(tr("Folders are ignored; drop log files instead."), 6000);
    }
    if (paths.isEmpty())
        return;

    openPaths(paths, m_forcedFormatId);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    } else if (event->type() == QEvent::WindowStateChange) {
        applyFullScreenState();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setFullScreen(bool fullScreen)
{
    // showFullScreen() / showMaximized() / showNormal() are used instead of
    // setWindowState(): a maximized window keeps its WindowMaximized flag in
    // the state combination, which makes the full screen window drift off the
    // screen on Windows (right edge clipped). applyFullScreenState() runs from
    // changeEvent() for every transition, also the ones triggered by the
    // window system.
    if (fullScreen) {
        m_wasMaximizedBeforeFullScreen = isMaximized();
        // Remember the window geometry so closing while full screen does not
        // persist the full screen state (REQ-UI-14).
        m_geometryBeforeFullScreen = saveGeometry();
        showFullScreen();
    } else if (m_wasMaximizedBeforeFullScreen) {
        showMaximized();
    } else {
        showNormal();
    }
}

void MainWindow::applyFullScreenState()
{
    const bool fullScreen = isFullScreen();

    m_fullScreenAction->setChecked(fullScreen);
    m_escapeShortcut->setEnabled(fullScreen);

    if (fullScreen == m_fullScreenActive)
        return;                       // not a full screen transition
    m_fullScreenActive = fullScreen;

    if (fullScreen) {
        // Give the log table as much room as possible (REQ-UI-14); the panel
        // comes back when the user leaves full screen again.
        m_collapsedBeforeFullScreen = m_filterPanel->isCollapsed();
        m_filterPanel->setCollapsed(true);
    } else {
        m_filterPanel->setCollapsed(m_collapsedBeforeFullScreen);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Full screen is a temporary view mode (REQ-UI-14): store the geometry the
    // window had before entering it, never the full screen state itself.
    if (isFullScreen() && !m_geometryBeforeFullScreen.isEmpty())
        m_settings->setWindowGeometry(m_geometryBeforeFullScreen);
    else
        m_settings->setWindowGeometry(saveGeometry());
    m_settings->setWindowState(saveState());
    m_settings->setSplitterState(m_splitter->saveState());
    saveColumnLayout();
    QMainWindow::closeEvent(event);
}

void MainWindow::applyFonts()
{
    QApplication::setFont(m_settings->interfaceFont());
    m_tableView->setTableFont(m_settings->tableFont());
    m_tableView->setHeaderFont(m_settings->headerFont());
    m_detailPane->setMessageFont(m_settings->tableFont());
}

void MainWindow::applyTheme(bool dark)
{
    m_tableView->setDarkTheme(dark);
    m_detailPane->setDarkTheme(dark);
    applySnippetTheme();
}

void MainWindow::applyDetailsPosition()
{
    const bool right = m_settings->detailsPosition() == SettingsStore::DetailsPosition::Right;
    m_splitter->setOrientation(right ? Qt::Horizontal : Qt::Vertical);

    const int totalWidth = m_splitter->width() > 0 ? m_splitter->width() : width();
    const int totalHeight = m_splitter->height() > 0 ? m_splitter->height() : height();
    if (right)
        m_splitter->setSizes({static_cast<int>(totalWidth * 0.62), static_cast<int>(totalWidth * 0.38)});
    else
        m_splitter->setSizes({static_cast<int>(totalHeight * 0.68), static_cast<int>(totalHeight * 0.32)});
}

void MainWindow::applyDetailsBehavior()
{
    const bool showPane = m_settings->showDetailsPane();

    // Without the details pane the table is the only place to read an entry, so
    // nothing may be truncated: every row is sized to its complete content
    // (REQ-TABLE-04 exception, REQ-UI-11).
    m_tableView->setFullContentMode(!showPane);

    if (showPane) {
        if (m_provider && m_model->rowCount() > 0)
            selectFirstRowIfRequested();
    } else {
        m_detailPane->clearEntry();
        m_detailPane->hide();
    }

    // The pane position is meaningless while the pane is switched off.
    if (m_layoutMenu)
        m_layoutMenu->setEnabled(showPane);
    if (m_detailsRightAction)
        m_detailsRightAction->setEnabled(true);
    if (m_detailsBottomAction)
        m_detailsBottomAction->setEnabled(true);
}

void MainWindow::selectFirstRowIfRequested()
{
    if (!m_provider || !m_settings->showDetailsPane() || m_model->rowCount() <= 0)
        return;
    m_tableView->setCurrentIndex(m_model->index(0, 0));
    showEntryInDetailPane(0);
}

void MainWindow::onMonitorToggled(bool enabled)
{
    if (enabled)
        startMonitor();
    else
        stopMonitor();
}

void MainWindow::startMonitor()
{
    if (isEventLogChannelDocument()) {
        startEventLogMonitor();
        return;
    }

    if (!m_source || !m_watcher) {
        if (m_monitorAction->isChecked())
            m_monitorAction->setChecked(false);
        return;
    }
    m_watcher->start(m_source.get());
    m_tableView->setFollowSide(LogTableView::FollowSide::Bottom);
    m_tableView->setPendingNewRows(0);
    m_tableView->scrollToBottomNow();
    statusBar()->showMessage(tr("Monitoring %1 (tail -f)").arg(m_source->documentInfo().fileName),
                             4000);
    updateMonitorLabel();
}

bool MainWindow::isEventLogChannelDocument() const
{
    return !m_eventLogChannel.isEmpty() && !m_eventLogElevated;
}

bool MainWindow::isEventLogMonitoring() const
{
    return m_eventLogWatcher && m_eventLogWatcher->isActive();
}

void MainWindow::startEventLogMonitor()
{
    const auto provider = std::dynamic_pointer_cast<EventLogProvider>(m_provider);
    if (!provider || !m_eventLogBackend || m_eventLogChannel.isEmpty()) {
        if (m_monitorAction->isChecked())
            m_monitorAction->setChecked(false);
        return;
    }

    if (!m_eventLogWatcher) {
        m_eventLogWatcher = std::make_unique<EventLogWatcher>(this);
        connect(m_eventLogWatcher.get(), &EventLogWatcher::recordsPrepended, this,
                &MainWindow::onEventLogRowsPrepended);
        connect(m_eventLogWatcher.get(), &EventLogWatcher::documentRebuilt, this,
                &MainWindow::onEventLogDocumentRebuilt);
        connect(m_eventLogWatcher.get(), &EventLogWatcher::watchingFailed, this,
                &MainWindow::onEventLogWatchingFailed);
        connect(m_eventLogWatcher.get(), &EventLogWatcher::limitReached, this,
                &MainWindow::onEventLogLimitReached);
    }

    m_eventLogWatcher->start(m_eventLogBackend, provider, 2000);
    m_tableView->setFollowSide(LogTableView::FollowSide::Top);
    m_tableView->setPendingNewRows(0);
    m_tableView->scrollToTopNow();
    statusBar()->showMessage(tr("Monitoring %1 (every 2 s)").arg(m_eventLogChannel), 4000);
    updateMonitorLabel();
}

void MainWindow::stopMonitor()
{
    if (m_watcher)
        m_watcher->stop();
    if (m_eventLogWatcher)
        m_eventLogWatcher->stop();
    if (m_tableView)
        m_tableView->setPendingNewRows(0);
    updateMonitorLabel();
}

void MainWindow::updateMonitorLabel()
{
    if (!m_monitorLabel)
        return;
    const bool active = (m_eventLogWatcher && m_eventLogWatcher->isActive())
        || (m_watcher && m_watcher->isActive());
    if (!active) {
        m_monitorLabel->setText(tr("Monitor: off"));
        return;
    }
    const int pending = m_tableView ? m_tableView->pendingNewRows() : 0;
    if (pending > 0)
        m_monitorLabel->setText(tr("Monitor: on (%1 new events)").arg(pending));
    else
        m_monitorLabel->setText(tr("Monitor: on"));
}

void MainWindow::onRowsAppended(int firstRow, int count)
{
    const bool follow = m_tableView->isAtBottom();
    m_model->appendRows(firstRow);

    if (follow) {
        m_tableView->scrollToBottomNow();
    } else {
        m_tableView->setPendingNewRows(m_tableView->pendingNewRows() + count);
        statusBar()->showMessage(tr("%1 new lines (follow paused)").arg(count), 3000);
    }
    // Keep the level counters in sync without touching the user's selection.
    if (m_filterPanel)
        m_filterPanel->updateLevelCounts(m_provider->levelCounts());
    updateMonitorLabel();
}

void MainWindow::onEventLogRowsPrepended(int count)
{
    if (!m_provider || count <= 0)
        return;

    const bool follow = m_tableView->isAtTop();
    const int anchorRow = m_tableView->rowAt(0);
    const int inserted = m_model->prependRows(count);

    if (follow) {
        m_tableView->scrollToTopNow();
    } else if (anchorRow >= 0 && inserted > 0) {
        // Keep the row the user was reading at the same viewport position;
        // prepending shifts every existing view row down (REQ-EVT-11).
        m_tableView->scrollTo(m_model->index(anchorRow + inserted, 0),
                              QAbstractItemView::PositionAtTop);
    }

    if (!follow) {
        m_tableView->setPendingNewRows(m_tableView->pendingNewRows() + count);
        statusBar()->showMessage(tr("%1 new events (follow paused)").arg(count), 3000);
    }
    if (m_filterPanel)
        m_filterPanel->updateLevelCounts(m_provider->levelCounts());
    updateMonitorLabel();
}

void MainWindow::onEventLogDocumentRebuilt()
{
    const bool wasMonitoring = m_monitorAction && m_monitorAction->isChecked();
    statusBar()->showMessage(
        tr("The event log was cleared or recreated; the document was reloaded."), 6000);
    if (!loadEventLog(m_eventLogChannel, m_eventLogPath, m_eventLogMaxRecords, wasMonitoring)
        && m_monitorAction && m_monitorAction->isChecked()) {
        m_monitorAction->setChecked(false);
    }
}

void MainWindow::onEventLogLimitReached()
{
    if (m_monitorAction && m_monitorAction->isChecked())
        m_monitorAction->setChecked(false);
    statusBar()->showMessage(
        tr("Live monitoring paused: %1 new events reached the limit.")
            .arg(EventLogWatcher::kMaxAddedRecords),
        10000);
}

void MainWindow::onEventLogWatchingFailed(const QString &message)
{
    reportError(message);
    if (m_monitorAction && m_monitorAction->isChecked())
        m_monitorAction->setChecked(false);
}

void MainWindow::onDocumentRebuilt()
{
    if (!m_source)
        return;
    statusBar()->showMessage(
        tr("The log file was rotated or truncated; the document was reloaded."), 6000);
    m_tableView->setHeightPassSuspended(true);
    m_model->setProvider(m_source);
    m_tableView->clearExpansion();
    updateDocumentUi();
    applyDetailsBehavior();
    m_tableView->setHeightPassSuspended(false);
    selectFirstRowIfRequested();
    if (m_monitorAction->isChecked())
        m_tableView->scrollToBottomNow();
}

void MainWindow::onOpen()
{
    QString filters = tr("Log files (*.log *.txt *.out);;All files (*)");
    if (m_eventLogBackend && m_eventLogBackend->isSupported())
        filters = tr("Log files (*.log *.txt *.out);;Windows event logs (*.evtx);;All files (*)");
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Log Files"), QString(),
                                                            filters);
    if (files.isEmpty())
        return;
    openPaths(files, m_forcedFormatId);
}

void MainWindow::onOpenEventLogChannels()
{
    if (!m_eventLogBackend || !m_eventLogBackend->isSupported())
        return;

    EventLogChannelDialog dialog(m_eventLogBackend, this);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedChannel().isEmpty())
        openEventLogChannel(dialog.selectedChannel());
}

void MainWindow::onOpenEventLogFile()
{
    if (!m_eventLogBackend || !m_eventLogBackend->isSupported())
        return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Windows Event Log File"), QString(),
        tr("Windows event logs (*.evtx);;All files (*)"));
    if (!path.isEmpty())
        openEventLogFile(path);
}

void MainWindow::openEventLogChannel(const QString &channel)
{
    loadEventLog(channel, QString(), EventLogProvider::kMaxRecords);
}

void MainWindow::openEventLogFile(const QString &path)
{
    loadEventLog(QString(), path, EventLogProvider::kMaxRecords);
}

bool MainWindow::loadEventLog(const QString &channel, const QString &path, int maxRecords,
                              bool resumeMonitoring)
{
    if (!m_eventLogBackend || !m_eventLogBackend->isSupported())
        return false;
    if (channel.isEmpty() == path.isEmpty())   // exactly one target
        return false;

    QElapsedTimer loadTimer;
    loadTimer.start();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    EventLogError errorKind = EventLogError::None;
    const auto provider = channel.isEmpty()
        ? EventLogProvider::openFile(m_eventLogBackend, path, maxRecords, &error, &errorKind)
        : EventLogProvider::openChannel(m_eventLogBackend, channel, maxRecords, &error,
                                        &errorKind);
    QApplication::restoreOverrideCursor();

    if (!provider) {
        // Keep the current document (and its "load more" target) intact.
        reportError(error);
        if (errorKind == EventLogError::AccessDenied) {
            // Offer the UAC helper (REQ-EVT-10); reportError() cleared the
            // previous offer, so store the target afterwards.
            m_pendingEventLogElevationChannel = channel;
            m_pendingEventLogElevationPath = path;
            if (m_elevateButton) {
                const QString target = channel.isEmpty() ? path : channel;
                m_elevateButton->setText(tr("Open as Administrator…"));
                m_elevateButton->setToolTip(
                    tr("Read \"%1\" with administrator rights (UAC).").arg(target));
                m_elevateButton->show();
            }
        }
        return false;
    }

    m_eventLogElevated = false;
    // A native event log replaces a pending elevated snapshot and is not a
    // regular file: Refresh stays disabled through setProvider().
    releaseSnapshot();
    clearElevationOffer();
    m_forcedFormatId.clear();
    setProvider(provider, QString());
    setLoadTime(loadTimer.elapsed());

    if (provider->documentInfo().truncated) {
        statusBar()->showMessage(
            tr("Showing the newest %1 events; older events are not loaded.")
                .arg(provider->rowCount()),
            8000);
    }

    // "Load more" / channel rebuild: keep the user's monitor switch on and
    // restart polling against the fresh provider (REQ-EVT-09/11).
    if (resumeMonitoring && isEventLogChannelDocument())
        m_monitorAction->setChecked(true);

    return true;
}

void MainWindow::loadElevatedEventLog(const QString &channel, const QString &path, int maxRecords)
{
    if (!ElevatedEventLog::isSupported() || (channel.isEmpty() == path.isEmpty()))
        return;

    QElapsedTimer loadTimer;
    loadTimer.start();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const EventLogReadResult result =
        ElevatedEventLog::read(channel, path, maxRecords, &error);
    QApplication::restoreOverrideCursor();

    if (!result.error.isEmpty()) {
        reportError(result.error.isEmpty() ? error : result.error);
        // Keep the offer clickable for another attempt.
        m_pendingEventLogElevationChannel = channel;
        m_pendingEventLogElevationPath = path;
        if (m_elevateButton)
            m_elevateButton->show();
        return;
    }

    const QFileInfo fileInfo(path);
    const QString fileName = channel.isEmpty() ? fileInfo.fileName() : channel;
    const QString filePath = channel.isEmpty() ? fileInfo.absoluteFilePath() : QString();
    const auto provider =
        EventLogProvider::fromResult(channel, fileName, filePath, maxRecords, result);

    releaseSnapshot();
    clearElevationOffer();
    m_forcedFormatId.clear();
    m_eventLogElevated = true;
    setProvider(provider, QString());
    setLoadTime(loadTimer.elapsed());

    if (result.truncated) {
        statusBar()->showMessage(
            tr("Showing the newest %1 events; older events are not loaded.")
                .arg(provider->rowCount()),
            8000);
    }
}

void MainWindow::onLoadMoreEvents()
{
    if (!m_provider || (m_eventLogChannel.isEmpty() && m_eventLogPath.isEmpty()))
        return;

    const int loaded = m_provider->rowCount();
    const bool wasMonitoring = m_monitorAction && m_monitorAction->isChecked();

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Truncated Event Log"));
    box.setText(tr("Only the newest %1 events are loaded.").arg(loaded));
    QPushButton *more = box.addButton(
        tr("Load %1 More").arg(EventLogProvider::kMaxRecords), QMessageBox::AcceptRole);
    QPushButton *all = box.addButton(tr("Load All Events…"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == more) {
        const int next = m_eventLogMaxRecords > 0
            ? m_eventLogMaxRecords + EventLogProvider::kMaxRecords
            : EventLogProvider::kMaxRecords;
        if (m_eventLogElevated)
            loadElevatedEventLog(m_eventLogChannel, m_eventLogPath, next);
        else
            loadEventLog(m_eventLogChannel, m_eventLogPath, next, wasMonitoring);
    } else if (box.clickedButton() == all) {
        const auto answer = QMessageBox::warning(
            this, tr("Load All Events"),
            tr("Loading every event can take a long time and use a lot of memory. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            if (m_eventLogElevated)
                loadElevatedEventLog(m_eventLogChannel, m_eventLogPath, 0);
            else
                loadEventLog(m_eventLogChannel, m_eventLogPath, 0, wasMonitoring);
        }
    }
}

void MainWindow::onRefresh()
{
    // An elevated document lives in a snapshot: re-read the original file with
    // fresh authorization instead of refreshing the snapshot (REQ-REL-04).
    if (!m_elevatedOriginalPath.isEmpty()) {
        openElevated(m_elevatedOriginalPath);
        return;
    }
    if (m_currentPath.isEmpty())
        return;
    openPaths({m_currentPath}, m_forcedFormatId);
}

void MainWindow::onCloseDocument()
{
    stopMonitor();
    if (m_monitorAction)
        m_monitorAction->setChecked(false);
    m_source.reset();
    m_provider.reset();
    m_currentPath.clear();
    m_eventLogChannel.clear();
    m_eventLogPath.clear();
    m_eventLogMaxRecords = 0;
    m_eventLogElevated = false;
    clearElevationOffer();
    releaseSnapshot();
    m_tableView->clearExpansion();
    m_model->setProvider(nullptr);
    m_detailPane->clearEntry();
    m_detailPane->hide();
    updateDocumentUi();
}

void MainWindow::onAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onToggleAssociation(bool checked)
{
    const QString extension = FileAssociation::defaultExtension();
    const QString exePath = QCoreApplication::applicationFilePath();

    if (checked) {
        const FileAssociation::Result result =
            FileAssociation::registerForCurrentUser(extension, exePath);
        if (!result.ok) {
            QMessageBox::warning(this, tr("File Association"), result.error);
        } else {
            QString text = tr("Double-clicking a %1 file now opens:\n%2").arg(extension, exePath);
            for (const QString &warning : result.warnings)
                text += QLatin1Char('\n') + warning;
            QMessageBox::information(this, tr("File Association"), text);
        }
    } else {
        const FileAssociation::Result result = FileAssociation::unregisterForCurrentUser(extension);
        if (!result.ok) {
            QMessageBox::warning(this, tr("File Association"), result.error);
        } else {
            QMessageBox::information(
                this, tr("File Association"),
                tr("The %1 association was removed and the previous one restored.").arg(extension));
        }
    }

    // The registry is the source of truth: a failed registration must not leave
    // a check mark behind.
    syncAssociationAction();
}

void MainWindow::syncAssociationAction()
{
    const bool registered = FileAssociation::isRegisteredForCurrentUser(
        FileAssociation::defaultExtension(), QCoreApplication::applicationFilePath());
    if (registered == m_associateAction->isChecked())
        return;
    const QSignalBlocker blocker(m_associateAction);
    m_associateAction->setChecked(registered);
}

void MainWindow::onCurrentRowChanged(const QModelIndex &current)
{
    // The pane is never opened here: only an explicit user activation
    // (onRowActivatedByUser) or the "always show details" option opens it. This
    // keeps programmatic selection changes from popping up the pane.
    if (!m_detailPane->isVisible())
        return;
    if (!current.isValid() || !m_model->entry(current.row())) {
        m_detailPane->clearEntry();
        return;
    }
    const DocumentInfo info = m_provider ? m_provider->documentInfo() : DocumentInfo();
    m_detailPane->setEntry(m_model->entry(current.row()), info.fileName,
                           info.formatId == QLatin1String("wevt_native"));
}

void MainWindow::onRowActivatedByUser(int row)
{
    // With the pane switched off a click must not open it (spec.md REQ-UI-11).
    updateFindCounter();
    if (!m_settings->showDetailsPane())
        return;
    showEntryInDetailPane(row);
}

void MainWindow::showEntryInDetailPane(int row)
{
    const LogEntry *entry = m_model->entry(row);
    if (!entry)
        return;
    const DocumentInfo info = m_provider ? m_provider->documentInfo() : DocumentInfo();
    m_detailPane->setEntry(entry, info.fileName, info.formatId == QLatin1String("wevt_native"));
    showDetailPane();
}

bool MainWindow::isDetailPaneVisible() const
{
    return m_detailPane->isVisible();
}

bool MainWindow::isFullContentMode() const
{
    return m_tableView->fullContentMode();
}

int MainWindow::currentRow() const
{
    const QModelIndex current = m_tableView->currentIndex();
    return current.isValid() ? current.row() : -1;
}

void MainWindow::showDetailPane()
{
    if (m_detailPane->isVisible())
        return;
    const bool firstShow = m_splitter->sizes().value(1, 0) == 0;
    m_detailPane->show();
    if (firstShow)
        applyDetailsPosition();
}

void MainWindow::onCopyFeedback(const QString &message)
{
    statusBar()->showMessage(message, 4000);
}

void MainWindow::applyFindSettings()
{
    QString error;
    const Matcher matcher = Matcher::build(m_filterPanel->findText(),
                                          static_cast<MatchMode>(m_filterPanel->findMode()),
                                          m_filterPanel->findCaseSensitive(), &error);
    if (!matcher.isValid()) {
        statusBar()->showMessage(tr("Invalid pattern: %1").arg(error), 6000);
        return;
    }
    m_model->setFindMatcher(matcher);
    m_detailPane->setFindHighlight(matcher, m_settings->highlightBackground(),
                                   m_settings->highlightForeground());
    m_findOrdinal = 0;
    if (!matcher.isEmpty())
        gotoMatch(1);          // jump to the first match like most viewers do
    updateFindCounter();
    m_tableView->viewport()->update();
}

void MainWindow::applyFilterSettings()
{
    FilterSpec spec;
    // Selecting every level that occurs in the document means "no level filter"
    // (keeps the full speed path of the model).
    spec.levelMask = m_filterPanel->allAvailableLevelsChecked()
        ? 0
        : m_filterPanel->checkedLevelMask();
    spec.timeRangeActive = m_filterPanel->timeRangeActive();
    spec.timeFrom = m_filterPanel->timeFrom();
    spec.timeTo = m_filterPanel->timeTo();
    spec.includeInvalidTime = false;

    QString error;
    spec.keyword = Matcher::build(m_filterPanel->filterText(),
                                  static_cast<MatchMode>(m_filterPanel->filterMode()),
                                  m_filterPanel->filterCaseSensitive(), &error);
    if (!spec.keyword.isValid()) {
        statusBar()->showMessage(tr("Invalid pattern: %1").arg(error), 6000);
        return;
    }
    // Inverted filtering: only rows without a match stay visible (REQ-FILTER-09).
    spec.invertKeyword = m_filterPanel->filterInverted();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_model->setFilterSpec(spec);
    QApplication::restoreOverrideCursor();

    m_detailPane->clearEntry();
    m_tableView->clearExpansion();
    m_findOrdinal = 0;
    updateDocumentUi(false);     // keep the level selection untouched
    updateFindCounter();
    // In "always show details" mode keep the pane filled after filtering.
    if (m_settings->showDetailsPane() && m_model->rowCount() > 0)
        selectFirstRowIfRequested();
}

bool MainWindow::findPatternIsPending() const
{
    const Matcher applied = m_model->findMatcher();
    return m_filterPanel->findText() != applied.pattern()
        || static_cast<MatchMode>(m_filterPanel->findMode()) != applied.mode()
        || m_filterPanel->findCaseSensitive() != applied.caseSensitive();
}

void MainWindow::gotoMatch(int direction)
{
    // A pattern that was typed but not committed yet is applied first, so F3 and
    // the navigation buttons work without pressing Enter (REQ-FIND-07).
    if (findPatternIsPending()) {
        applyFindSettings();     // jumps to the first match
        return;
    }

    const int total = m_model->findMatchRowCount();
    if (total <= 0) {
        updateFindCounter();
        return;
    }

    int ordinal = m_findOrdinal > 0 ? m_findOrdinal - 1 : (direction > 0 ? -1 : 0);
    ordinal += direction;
    ordinal = ((ordinal % total) + total) % total;

    const int viewRow = m_model->findRowAt(ordinal);
    if (viewRow >= 0) {
        const int messageColumn = qMax(0, m_tableView->messageColumnIndex());
        const QModelIndex index = m_model->index(viewRow, messageColumn);
        m_tableView->setCurrentIndex(index);
        m_tableView->scrollTo(index, QAbstractItemView::PositionAtCenter);
    }
    m_findOrdinal = ordinal + 1;
    updateFindCounter();
}

void MainWindow::updateFindCounter()
{
    const int total = m_model->findMatchRowCount();
    int ordinal = 0;
    if (total > 0 && m_tableView->currentIndex().isValid())
        ordinal = qMax(0, m_model->findOrdinalOf(m_tableView->currentIndex().row()));
    if (ordinal > 0)
        m_findOrdinal = ordinal;
    m_filterPanel->setFindCounter(m_findOrdinal, total);
}

void MainWindow::onExportFiltered()
{
    if (!m_provider)
        return;

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Filtered Results"), QString(),
        tr("CSV files (*.csv);;Text files (*.txt)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        reportError(tr("Cannot write '%1': %2").arg(path, file.errorString()));
        return;
    }

    const bool csv = path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive);
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    const auto escapeCsv = [](const QString &value) {
        QString escaped = value;
        escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));
        return QLatin1Char('"') + escaped + QLatin1Char('"');
    };

    const int columns = m_model->columnCount();
    if (csv) {
        QStringList titles;
        for (int column = 0; column < columns; ++column) {
            if (m_model->isColumnHidden(column))
                continue;
            titles << escapeCsv(m_model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
        }
        out << titles.join(QLatin1Char(',')) << '\n';
    }

    const int rows = m_model->rowCount();
    for (int row = 0; row < rows; ++row) {
        QStringList cells;
        for (int column = 0; column < columns; ++column) {
            if (m_model->isColumnHidden(column))
                continue;   // export what the table shows (REQ-TABLE-11)
            const QString text = m_model->cellText(row, column);
            cells << (csv ? escapeCsv(text) : text);
        }
        out << cells.join(csv ? QStringLiteral(",") : QStringLiteral("\t")) << '\n';
    }
    file.close();

    statusBar()->showMessage(tr("Exported %1 rows to %2").arg(rows).arg(path), 8000);
}

void MainWindow::applyHighlightColors()
{
    m_tableView->setKeywordColors(m_settings->highlightBackground(),
                                  m_settings->highlightForeground());
    m_detailPane->setFindHighlight(m_model->findMatcher(), m_settings->highlightBackground(),
                                   m_settings->highlightForeground());
    m_tableView->viewport()->update();
}

void MainWindow::applySnippetTheme()
{
    const HighlightTheme &theme = currentHighlightTheme();
    m_tableView->setHighlightTheme(&theme);
    m_detailPane->setHighlightTheme(&theme);
    m_tableView->viewport()->update();
}

const HighlightTheme &MainWindow::currentHighlightTheme() const
{
    const QString id = m_settings->syntaxTheme();
    if (id == QLatin1String("dark+"))
        return HighlightTheme::vscodeDark();
    if (id == QLatin1String("light+"))
        return HighlightTheme::vscodeLight();
    return HighlightTheme::forDarkMode(m_theme->isDark());
}

void MainWindow::chooseFont(int role)
{
    bool ok = false;
    QFont initial;
    QString title;
    switch (role) {
    case InterfaceFontRole:
        initial = m_settings->interfaceFont();
        title = tr("Interface Font");
        break;
    case TableFontRole:
        initial = m_settings->tableFont();
        title = tr("Log Table Font");
        break;
    default:
        initial = m_settings->headerFont();
        title = tr("Log Header Font");
        break;
    }

    const QFont font = QFontDialog::getFont(&ok, initial, this, title);
    if (!ok)
        return;
    switch (role) {
    case InterfaceFontRole:
        m_settings->setInterfaceFont(font);
        break;
    case TableFontRole:
        m_settings->setTableFont(font);
        break;
    default:
        m_settings->setHeaderFont(font);
        break;
    }
}

void MainWindow::chooseHighlightColor()
{
    const QColor color = QColorDialog::getColor(m_settings->highlightBackground(), this,
                                                tr("Highlight Color"));
    if (color.isValid())
        m_settings->setHighlightBackground(color);
}

void MainWindow::setProvider(const EntryProviderPtr &provider, const QString &path)
{
    // Remember the column layout of the previous document.
    saveColumnLayout();     // widths and hidden columns of the outgoing document

    m_provider = provider;
    m_currentPath = path;

    // Track the native event log target so "Load More Events…" can requery it
    // (REQ-EVT-09); any other provider clears the state.
    m_eventLogChannel.clear();
    m_eventLogPath.clear();
    m_eventLogMaxRecords = 0;
    if (auto eventLog = std::dynamic_pointer_cast<EventLogProvider>(provider)) {
        m_eventLogChannel = eventLog->channelName();
        m_eventLogPath = eventLog->documentInfo().filePath;
        m_eventLogMaxRecords = eventLog->requestedLimit();
    } else {
        m_eventLogElevated = false;
    }

    // Live monitoring works for single file documents only (REQ-MON-01).
    m_source.reset();
    if (auto document = std::dynamic_pointer_cast<LogDocument>(provider)) {
        if (document->isSingleFile())
            m_source = document->sourcePtr(0);
    } else if (auto source = std::dynamic_pointer_cast<LogSource>(provider)) {
        m_source = source;
    }

    // Replacing the document always ends the previous monitor. loadEventLog()
    // explicitly restarts it for "load more"/channel rebuild (REQ-EVT-09/11).
    if (m_monitorAction && m_monitorAction->isChecked())
        m_monitorAction->setChecked(false);
    stopMonitor();
    m_tableView->setFollowSide(isEventLogChannelDocument()
                                   ? LogTableView::FollowSide::Top
                                   : LogTableView::FollowSide::Bottom);

    m_lastError.clear();
    if (m_warningLabel)
        m_warningLabel->hide();
    updateEmptyHint();
    m_tableView->clearExpansion();

    // Attaching a document touches the model, the column visibility, the widths
    // and the details pane in turn; every one of those steps used to trigger its
    // own row height pass. Suspend the passes and run a single one at the end.
    m_tableView->setHeightPassSuspended(true);

    m_model->setProvider(provider);

    // Signature of the column layout: format id + column titles.
    QStringList titles;
    for (int column = 0; column < m_model->columnCount(); ++column) {
        titles << m_model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
    }
    m_documentSignature = (provider ? provider->documentInfo().formatId : QString())
        + QLatin1Char('|') + titles.join(QLatin1Char(','));

    const QStringList hidden = m_settings->hiddenColumns(m_documentSignature);
    m_model->setHiddenColumnKeys(QSet<QString>(hidden.begin(), hidden.end()));
    m_tableView->applyColumnVisibility();
    m_tableView->applyColumnWidths();
    m_tableView->restoreColumnWidths(m_settings->columnWidths(m_documentSignature));
    rebuildColumnsMenu();
    m_detailPane->clearEntry();
    if (!m_settings->showDetailsPane())
        m_detailPane->hide();
    updateDocumentUi(true);      // a new document re-syncs the level selection
    applyDetailsBehavior();
    m_tableView->setHeightPassSuspended(false);
    selectFirstRowIfRequested();
}

void MainWindow::rebuildColumnsMenu()
{
    m_columnsMenu->clear();
    m_columnActions.clear();
    m_showAllColumnsAction = nullptr;

    const int columns = m_provider ? m_model->columnCount() : 0;
    for (int column = 0; column < columns; ++column) {
        QString title = m_model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        if (title.isEmpty())
            title = tr("Column %1").arg(column + 1);
        QAction *action = m_columnsMenu->addAction(title);
        action->setCheckable(true);
        action->setChecked(!m_model->isColumnHidden(column));
        m_columnActions.append(action);

        if (m_model->columnKind(column) == ColumnKind::Line) {
            // The Line column anchors the table to the raw file and is always
            // shown (REQ-PARSE-03), so its item stays checked but disabled.
            action->setEnabled(false);
            action->setToolTip(tr("The Line column is always shown"));
            continue;
        }

        connect(action, &QAction::triggered, this, [this, action, column](bool checked) {
            m_model->setColumnHidden(column, !checked);
            action->setChecked(!m_model->isColumnHidden(column));
            syncColumnsMenu();
            saveColumnLayout();
        });
    }

    if (columns > 0) {
        m_columnsMenu->addSeparator();
        m_showAllColumnsAction = m_columnsMenu->addAction(tr("Show All Columns"));
        connect(m_showAllColumnsAction, &QAction::triggered, this, [this] {
            for (int column = 0; column < m_model->columnCount(); ++column)
                m_model->setColumnHidden(column, false);
            syncColumnsMenu();
            saveColumnLayout();
        });
    }

    m_columnsMenu->setEnabled(columns > 0);
    syncColumnsMenu();
}

void MainWindow::syncColumnsMenu()
{
    for (int column = 0; column < m_columnActions.size(); ++column)
        m_columnActions.at(column)->setChecked(!m_model->isColumnHidden(column));

    if (m_showAllColumnsAction) {
        bool anyHidden = false;
        for (int column = 0; column < m_model->columnCount(); ++column)
            anyHidden = anyHidden || m_model->isColumnHidden(column);
        m_showAllColumnsAction->setEnabled(anyHidden);
    }
}

void MainWindow::saveColumnLayout()
{
    if (m_documentSignature.isEmpty())
        return;
    m_settings->setColumnWidths(m_documentSignature, m_tableView->columnWidths());
    QStringList hidden = m_model->hiddenColumnKeys().values();
    hidden.sort();
    m_settings->setHiddenColumns(m_documentSignature, hidden);
}

void MainWindow::updateDocumentUi(bool documentLoaded)
{
    const bool hasDocument = static_cast<bool>(m_provider);
    m_logStack->setCurrentIndex(hasDocument ? 1 : 0);

    if (!hasDocument) {
        m_docLabel->setText(tr("No file open"));
        m_statsLabel->clear();
        setLoadTime(-1);
        setWindowTitle(QStringLiteral("Log Viewer"));
        m_filterPanel->setLevelCounts({});
        m_refreshAction->setEnabled(false);
        m_closeAction->setEnabled(false);
        m_exportAction->setEnabled(false);
        m_monitorAction->setEnabled(false);
        m_loadMoreEventsAction->setEnabled(false);
        m_loadMoreEventsButton->hide();
        m_columnsMenu->clear();
        m_columnActions.clear();
        m_showAllColumnsAction = nullptr;
        m_columnsMenu->setEnabled(false);
        if (m_monitorAction->isChecked())
            m_monitorAction->setChecked(false);
        updateMonitorLabel();
        return;
    }

    const DocumentInfo info = m_provider->documentInfo();
    QString documentText = info.fileName.isEmpty() ? tr("Log document") : info.fileName;
    if (info.truncated)
        documentText += QLatin1Char(' ') + tr("(truncated)");
    if (!m_snapshotPath.isEmpty())
        documentText += QLatin1Char(' ') + tr("(elevated snapshot)");
    if (m_eventLogElevated)
        documentText += QLatin1Char(' ') + tr("(elevated)");
    m_docLabel->setText(documentText);

    QStringList stats;
    if (!info.formatName.isEmpty())
        stats << info.formatName;
    if (!info.encodingName.isEmpty())
        stats << info.encodingName;
    if (m_model->isFiltered())
        stats << tr("%1 of %2 lines").arg(m_model->rowCount()).arg(m_provider->rowCount());
    else
        stats << tr("%1 lines").arg(info.lineCount);
    m_statsLabel->setText(stats.join(QStringLiteral(" · ")));

    setWindowTitle(QStringLiteral("Log Viewer — %1").arg(info.fileName));
    if (documentLoaded)
        m_filterPanel->setLevelCounts(m_provider->levelCounts());
    else
        m_filterPanel->updateLevelCounts(m_provider->levelCounts());
    m_refreshAction->setEnabled(!m_currentPath.isEmpty());
    m_closeAction->setEnabled(true);
    m_exportAction->setEnabled(true);

    // "Load More Events…" is offered only for a truncated native event log
    // (REQ-EVT-09).
    const bool canLoadMore = info.truncated
        && (!m_eventLogChannel.isEmpty() || !m_eventLogPath.isEmpty());
    m_loadMoreEventsAction->setEnabled(canLoadMore);
    m_loadMoreEventsButton->setVisible(canLoadMore);

    // Live monitoring needs a real file behind the document (no demo data) and
    // is disabled for elevated snapshots (REQ-REL-04). Native event log
    // channels are polled through EventLogWatcher (REQ-EVT-11).
    const bool canMonitor = (static_cast<bool>(m_source) && m_snapshotPath.isEmpty())
        || isEventLogChannelDocument();
    m_monitorAction->setEnabled(canMonitor);
    if (!canMonitor && m_monitorAction->isChecked())
        m_monitorAction->setChecked(false);
    updateMonitorLabel();
}

void MainWindow::loadDemoData()
{
    setProvider(createDemoProvider(), QString());
    setLoadTime(-1);      // demo data is not a file open (REQ-UI-15)
    statusBar()->showMessage(tr("Demo data loaded. Filtering and monitoring arrive in later "
                               "milestones."), 6000);
}

void MainWindow::setLoadTime(qint64 milliseconds)
{
    m_lastLoadMs = milliseconds;
    updateLoadTimeLabel();
}

void MainWindow::updateLoadTimeLabel()
{
    if (!m_loadTimeLabel)
        return;
    m_loadTimeLabel->setText(m_lastLoadMs >= 0
                                 ? tr("Loaded in %1").arg(formatDuration(m_lastLoadMs))
                                 : QString());
}

void MainWindow::updateEmptyHint()
{
    const bool hasError = !m_lastError.isEmpty();

    // The empty state doubles as the error surface when no document is loaded
    // (REQ-REL-01): a dynamic property switches the label to the themed warning
    // colour and back to the regular one.
    if (m_emptyHint->property("warning").toBool() != hasError) {
        m_emptyHint->setProperty("warning", hasError);
        m_emptyHint->style()->unpolish(m_emptyHint);
        m_emptyHint->style()->polish(m_emptyHint);
    }

    m_emptyHint->setText(hasError
                             ? m_lastError
                             : tr("Open a log file with File ▸ Open, drop one onto this window, "
                                  "or start the application with a file name.\nRun "
                                  "\"log-viewer --demo\" to preview the interface with sample data."));
}

void MainWindow::openPaths(const QStringList &paths, const QString &forcedFormatId)
{
    // Archived/copied event logs are binary; route them to the native API
    // instead of the text parser (REQ-EVT-03). Multi-select cannot merge event
    // logs: one document holds exactly one channel or file.
    for (const QString &path : paths) {
        if (!path.endsWith(QLatin1String(".evtx"), Qt::CaseInsensitive))
            continue;
        if (!m_eventLogBackend || !m_eventLogBackend->isSupported()) {
            reportError(tr("Windows event logs are only available on Windows."));
            return;
        }
        if (paths.size() != 1) {
            reportError(tr("Open one Windows event log file at a time."));
            return;
        }
        openEventLogFile(path);
        return;
    }

    const QString currentPath = paths.size() == 1 ? paths.first() : QString();
    openPathsInternal(paths, forcedFormatId, paths, currentPath, false);
}

bool MainWindow::openPathsInternal(const QStringList &paths, const QString &forcedFormatId,
                                   const QStringList &recentPaths, QString currentPath,
                                   bool keepSnapshot)
{
    if (paths.isEmpty())
        return false;

    m_forcedFormatId = forcedFormatId;

    const auto openWith = [this, &paths, &forcedFormatId](int maxLines, EntryProviderPtr *provider,
                                                          QString *error, bool *timeMergeDisabled) {
        *error = QString();
        if (timeMergeDisabled)
            *timeMergeDisabled = false;

        if (paths.size() == 1) {
            // Single file: the source itself is the provider, which keeps live
            // monitoring working (the watcher refreshes the source in place).
            auto source = LogSource::open(paths.first(), forcedFormatId, error, maxLines,
                                          m_settings->continuationMerge());
            if (!source)
                return false;
            *provider = source;
            return true;
        }

        const LogDocument::OpenResult opened = LogDocument::open(
            paths, forcedFormatId, maxLines, m_settings->continuationMerge());
        if (!opened.document) {
            *error = opened.error;
            return false;
        }
        if (timeMergeDisabled)
            *timeMergeDisabled = opened.timeMergeDisabled;
        *provider = opened.document;
        return true;
    };

    EntryProviderPtr provider;
    QString error;
    bool timeMergeDisabled = false;

    // Measure the whole open (parsing/indexing plus model attachment) for the
    // status bar label (REQ-UI-15). The "load the complete file?" dialog is
    // excluded: it is user think time, not load time.
    QElapsedTimer loadTimer;
    loadTimer.start();
    qint64 dialogMs = 0;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = openWith(m_settings->maxLinesPerFile(), &provider, &error, &timeMergeDisabled);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        reportError(error);
        if (!keepSnapshot)
            updateElevationOffer(recentPaths);
        return false;
    }

    bool wasTruncated = false;
    if (provider)
        wasTruncated = provider->documentInfo().truncated;

    if (wasTruncated) {
        const int limit = m_settings->maxLinesPerFile();
        QElapsedTimer dialogTimer;
        dialogTimer.start();
        const auto answer = QMessageBox::question(
            this, tr("Large file"),
            tr("The file contains more than %1 lines; only the first %1 lines are loaded.\n\n"
               "Load the complete file now?")
                .arg(limit),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        dialogMs += dialogTimer.elapsed();
        if (answer == QMessageBox::Yes) {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            EntryProviderPtr fullProvider;
            QString fullError;
            const bool fullOk = openWith(0, &fullProvider, &fullError, nullptr);
            QApplication::restoreOverrideCursor();
            if (fullOk)
                provider = fullProvider;
            else
                reportError(fullError);
        }
    }

    if (timeMergeDisabled) {
        statusBar()->showMessage(
            tr("Some files have no parsable timestamps; they are shown one after another."), 7000);
    }

    // A regular open replaces the elevated snapshot of the previous document
    // (REQ-REL-04); the elevated flow keeps the snapshot it just registered.
    if (!keepSnapshot)
        releaseSnapshot();
    clearElevationOffer();

    for (const QString &file : recentPaths)
        m_settings->addRecentFile(file);
    updateRecentFilesMenu();
    setProvider(provider, currentPath);
    setLoadTime(loadTimer.elapsed() - dialogMs);
    return true;
}

void MainWindow::openElevated(const QString &path)
{
    if (path.isEmpty())
        return;

    // The status bar offer (and with it m_elevationCandidate) is cleared while
    // the document is attached; a local copy keeps the requested path valid for
    // the whole operation.
    const QString requestedPath = path;

    // Keep the current snapshot until the new document is actually loaded: a
    // cancelled authorization must not tear down the open document.
    const QString previousSnapshot = m_snapshotPath;
    const QString previousOriginal = m_elevatedOriginalPath;

    // This blocks while the polkit dialog waits for the user and while the
    // helper streams the file. Acceptable for system logs (see §16.10).
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const QString snapshot = ElevatedFileReader::createSnapshot(requestedPath, &error);
    QApplication::restoreOverrideCursor();

    if (snapshot.isEmpty()) {
        reportError(tr("Could not open \"%1\" with elevated privileges: %2")
                        .arg(requestedPath, error));
        updateElevationOffer({requestedPath});     // keep the offer for another attempt
        return;
    }

    m_snapshotPath = snapshot;
    m_elevatedOriginalPath = requestedPath;
    if (!openPathsInternal({snapshot}, m_forcedFormatId, {requestedPath}, requestedPath, true)) {
        // The failure is already reported; drop the fresh snapshot and restore
        // the previous document state (if any).
        ElevatedFileReader::removeSnapshot(snapshot);
        m_snapshotPath = previousSnapshot;
        m_elevatedOriginalPath = previousOriginal;
        updateElevationOffer({requestedPath});
        return;
    }

    if (!previousSnapshot.isEmpty() && previousSnapshot != snapshot)
        ElevatedFileReader::removeSnapshot(previousSnapshot);
    statusBar()->showMessage(
        tr("Opened a read-only snapshot of \"%1\" with elevated privileges.").arg(requestedPath),
        8000);
}

void MainWindow::updateElevationOffer(const QStringList &paths)
{
    clearElevationOffer();

    // The first regular file the current user cannot read is the candidate.
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.exists() && info.isFile() && !info.isReadable()) {
            m_elevationCandidate = path;
            break;
        }
    }
    if (m_elevationCandidate.isEmpty())
        return;

    if (ElevatedFileReader::isSupported()) {
        m_elevateButton->setToolTip(
            tr("Read \"%1\" with administrator rights and open a read-only snapshot.")
                .arg(m_elevationCandidate));
        m_elevateButton->show();
        return;
    }

    // Without an authentication helper the alert stays visible; explain how to
    // grant read access manually (REQ-REL-04).
    const QString guidance = ElevatedFileReader::unsupportedText();
    if (m_warningLabel)
        m_warningLabel->setFullText(m_lastError + QLatin1Char('\n') + guidance);
    if (!m_provider)
        m_emptyHint->setText(m_lastError + QStringLiteral("\n\n") + guidance);
}

void MainWindow::clearElevationOffer()
{
    m_elevationCandidate.clear();
    m_pendingEventLogElevationChannel.clear();
    m_pendingEventLogElevationPath.clear();
    if (m_elevateButton)
        m_elevateButton->hide();
}

void MainWindow::releaseSnapshot()
{
    if (!m_snapshotPath.isEmpty()) {
        ElevatedFileReader::removeSnapshot(m_snapshotPath);
        m_snapshotPath.clear();
    }
    m_elevatedOriginalPath.clear();
}

void MainWindow::onElevateClicked()
{
    // Event log offer (REQ-EVT-10) takes precedence over the file snapshot.
    if (!m_pendingEventLogElevationChannel.isEmpty()
        || !m_pendingEventLogElevationPath.isEmpty()) {
        loadElevatedEventLog(m_pendingEventLogElevationChannel,
                             m_pendingEventLogElevationPath,
                             EventLogProvider::kMaxRecords);
        return;
    }
    if (!m_elevationCandidate.isEmpty())
        openElevated(m_elevationCandidate);
}

void MainWindow::reportError(const QString &message)
{
    m_lastError = message;
    qWarning("log-viewer: %s", qUtf8Printable(message));

    // A new failure invalidates the previous elevator candidate; openPaths()
    // re-creates the offer afterwards when the failure was a permission issue.
    clearElevationOffer();

    // REQ-REL-01: the alert has to stay visible. The status bar message area is
    // transient by design (showMessage() timeout, overwritten by the next status
    // text), so failures go into the persistent warning indicator instead.
    if (m_warningLabel) {
        m_warningLabel->setFullText(message);
        m_warningLabel->show();
    }

    if (!m_provider)
        m_logStack->setCurrentIndex(0);
    updateEmptyHint();
}

void MainWindow::updateRecentFilesMenu()
{
    m_recentMenu->clear();
    const QStringList files = m_settings->recentFiles();
    for (const QString &file : files) {
        auto *action = m_recentMenu->addAction(file);
        connect(action, &QAction::triggered, this, [this, file] { openPaths({file}); });
    }
    if (!files.isEmpty())
        m_recentMenu->addSeparator();
    m_recentMenu->addAction(m_clearRecentAction);
    m_recentMenu->setEnabled(!files.isEmpty());
}

void MainWindow::dumpLayout(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);
    writeLayout(out);
}

void MainWindow::writeLayout(QTextStream &out) const
{
    const auto describe = [&out](const char *name, const QWidget *widget) {
        if (!widget) {
            out << name << ": <null>\n";
            return;
        }
        const QSize hint = widget->minimumSizeHint();
        out << QStringLiteral("%1: size=%2x%3 minHint=%4x%5 visible=%6\n")
                   .arg(QLatin1String(name))
                   .arg(widget->width())
                   .arg(widget->height())
                   .arg(hint.width())
                   .arg(hint.height())
                   .arg(widget->isVisible() ? 1 : 0);
    };

    out << QStringLiteral("screen=%1x%2 dpr=%3\n")
               .arg(QApplication::primaryScreen() ? QApplication::primaryScreen()->geometry().width() : 0)
               .arg(QApplication::primaryScreen() ? QApplication::primaryScreen()->geometry().height() : 0)
               .arg(QApplication::primaryScreen() ? QApplication::primaryScreen()->devicePixelRatio() : 0.0)
               .arg(0.0, 0, 'f', 2);
    describe("window", this);
    describe("central", centralWidget());
    describe("content", m_content);
    describe("filterPanel", m_filterPanel);
    // Collapse toggle diagnostics (REQ-UI-13): the arrow lives in the title
    // row and is placed without a layout, so its geometry is worth checking.
    if (QToolButton *toggle = m_filterPanel->findChild<QToolButton *>(
            QStringLiteral("filterCollapseButton"))) {
        const QPoint global = toggle->mapToGlobal(QPoint(0, 0));
        out << QStringLiteral("filterCollapse collapsed=%1 button=%2,%3 %4x%5 global=%6,%7 "
                              "visRegionWidth=%8\n")
                   .arg(m_filterPanel->isCollapsed() ? 1 : 0)
                   .arg(toggle->x())
                   .arg(toggle->y())
                   .arg(toggle->width())
                   .arg(toggle->height())
                   .arg(global.x())
                   .arg(global.y())
                   .arg(toggle->visibleRegion().isEmpty()
                            ? 0 : toggle->visibleRegion().boundingRect().width());
    }
    describe("splitter", m_splitter);
    describe("logGroup", m_logGroup);
    describe("table", m_tableView);
    describe("detailPane", m_detailPane);
    // Status bar diagnostics (REQ-REL-01/04): warning label and elevated offer.
    describe("statusBar", statusBar());
    describe("elevateButton", m_elevateButton);
    out << QStringLiteral("tableColumns=%1 headerWidth=%2\n")
               .arg(m_tableView->model() ? m_tableView->model()->columnCount() : 0)
               .arg(m_tableView->horizontalHeader()->width());

    // Per column widths (auto-fit / column visibility diagnostics, REQ-TABLE-05).
    QStringList columnWidths;
    if (LogTableModel *columnModel = m_tableView->logModel()) {
        for (int column = 0; column < columnModel->columnCount(); ++column) {
            const QString title = columnModel->headerData(column, Qt::Horizontal,
                                                           Qt::DisplayRole).toString();
            columnWidths << QStringLiteral("%1=%2%3")
                                .arg(title)
                                .arg(m_tableView->columnWidth(column))
                                .arg(columnModel->isColumnHidden(column)
                                         ? QStringLiteral("(hidden)") : QString());
        }
    }
    out << QStringLiteral("columnWidths=%1\n").arg(columnWidths.join(QLatin1Char(',')));

    // Row sizing diagnostics (full content mode): applied vs freshly computed.
    LogTableModel *model = m_tableView->logModel();
    const int rows = model ? model->rowCount() : 0;
    out << QStringLiteral("fullContent=%1 rows=%2 rowHeightLines=%3\n")
               .arg(m_tableView->fullContentMode() ? 1 : 0)
               .arg(rows)
               .arg(m_settings->rowHeightLines());
    out << QStringLiteral("currentRow=%1 detailPaneVisible=%2\n")
               .arg(m_tableView->currentIndex().isValid() ? m_tableView->currentIndex().row() : -1)
               .arg(m_detailPane->isVisible() ? 1 : 0);
    int messageColumn = -1;
    for (int column = 0; model && column < model->columnCount(); ++column) {
        if (model->columnKind(column) == ColumnKind::Message)
            messageColumn = column;
    }
    out << QStringLiteral("messageColumn=%1 messageWidth=%2 viewportWidth=%3\n")
               .arg(messageColumn)
               .arg(messageColumn >= 0 ? m_tableView->columnWidth(messageColumn) : -1)
               .arg(m_tableView->viewport()->width());
    const int sample = qMin(8, rows);
    for (int row = 0; row < sample; ++row) {
        const QModelIndex index = model->index(row, qMax(0, messageColumn));
        const int applied = m_tableView->rowHeight(row);
        const int fresh = m_tableView->freshContentHeight(row);
        out << QStringLiteral("  row %1: applied=%2 fresh=%3 visualWidth=%4 messageLength=%5\n")
                   .arg(row)
                   .arg(applied)
                   .arg(fresh)
                   .arg(m_tableView->visualRect(index).width())
                   .arg(index.data(LogTableModel::FullTextRole).toString().size());
    }

    // Row sizing health: rows whose applied height is smaller than their content
    // height would be displayed truncated.
    int truncatedRows = 0;
    int tallestRow = -1;
    int tallestHeight = 0;
    for (int row = 0; row < rows; ++row) {
        const int applied = m_tableView->rowHeight(row);
        const int fresh = m_tableView->freshContentHeight(row);
        if (applied < fresh)
            ++truncatedRows;
        if (applied > tallestHeight) {
            tallestHeight = applied;
            tallestRow = row;
        }
    }
    out << QStringLiteral("truncatedRows=%1 tallestRow=%2 tallestHeight=%3\n")
               .arg(truncatedRows)
               .arg(tallestRow)
               .arg(tallestHeight);
    out.flush();
}

} // namespace lv
