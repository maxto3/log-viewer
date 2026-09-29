#include "app/SettingsStore.h"
#include "app/ThemeManager.h"
#include "app/TranslationManager.h"
#include "core/LogTableModel.h"
#include "highlight/SnippetTokenizer.h"
#include "platform/FileAssociation.h"
#include "ui/DetailPane.h"
#include "ui/FilterPanel.h"
#include "ui/LogItemDelegate.h"
#include "ui/LogTableView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTest>
#include <QTextOption>
#include <QToolButton>
#include <QUrl>

using namespace lv;

/// Behaviour tests for Settings ▸ Details Pane ▸ "Show Details Pane"
/// (spec.md REQ-UI-11): checked = the pane is always visible, unchecked = the
/// pane is never shown, not even when a row is clicked.
class TestMainWindowBehavior : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void uncheckedPaneStaysHidden();
    void uncheckedPaneShowsCompleteRows();
    void rowHeightCoversTheTallestColumn();
    void uncheckedPaneIgnoresRowClicks();
    void checkedSelectsFirstRow();
    void checkedPaneFollowsRowClicks();
    void checkedKeepsPaneOnReload();
    void reloadDoesNotOpenPaneWhenUnchecked();
    void uncheckingHidesPane();
    void detailsMessageWrapsWithoutHorizontalScrollBar();
    void monitorAppendsRowsAndFollows();
    void findHighlightsAndNavigates();
    void filterAppliesOnEnterOnly();
    void invertedFilterShowsNonMatchingRows();
    void levelFilterHidesNonMatchingRows();
    void levelListFollowsTheDocument();
    void zebraStripesAndGridLines();
    void filterPanelCollapsesToSingleRow();
    void fullScreenCollapsesFilterPanel();
    void columnsMenuHidesColumns();
    void autoFitColumnsFitsContent();
    void snippetColorsSurviveTruncation();
    void droppingFilesOpensAndMerges();
    void droppingAFolderIsIgnored();
    void loadTimeLabelShowsOpenDuration();
    void associationMenuEntryExists();
    void headersFollowLanguageSwitch();
    void resetAllKeepsDefaults();

private:
    QString settingsPath(const QString &name) const;
    std::unique_ptr<SettingsStore> makeSettings(const QString &name, bool showPane) const;
    static void clickRow(MainWindow &window, int row);

    QTemporaryDir m_dir;
};

void TestMainWindowBehavior::init()
{
    QVERIFY(m_dir.isValid());
}

QString TestMainWindowBehavior::settingsPath(const QString &name) const
{
    return QDir(m_dir.path()).filePath(name + QStringLiteral(".ini"));
}

std::unique_ptr<SettingsStore> TestMainWindowBehavior::makeSettings(const QString &name,
                                                                   bool showPane) const
{
    auto settings = std::make_unique<SettingsStore>(settingsPath(name));
    settings->setShowDetailsPane(showPane);
    return settings;
}

void TestMainWindowBehavior::clickRow(MainWindow &window, int row)
{
    LogTableView *view = window.logTableView();
    QVERIFY(view != nullptr);
    const QModelIndex index = view->model()->index(row, 0);
    QVERIFY(index.isValid());
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->visualRect(index).center());
}

void TestMainWindowBehavior::uncheckedPaneStaysHidden()
{
    auto settings = makeSettings(QStringLiteral("hidden"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.loadDemoData();

    QCOMPARE(window.currentRow(), -1);
    QVERIFY(!window.isDetailPaneVisible());
    // Without the details pane the table must show every row completely.
    QVERIFY(window.isFullContentMode());
}

void TestMainWindowBehavior::uncheckedPaneShowsCompleteRows()
{
    auto settings = makeSettings(QStringLiteral("complete-rows"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    QVERIFY(window.isFullContentMode());
    LogTableView *view = window.logTableView();
    LogTableModel *model = window.logModel();
    QVERIFY(view != nullptr && model != nullptr);

    const int messageColumn = view->messageColumnIndex();
    QVERIFY(messageColumn >= 0);

    // Every row must be at least as tall as its complete content, i.e. nothing is
    // truncated when the details pane is hidden.
    int longestRow = 0;
    int shortestRow = 0;
    int longestLength = 0;
    int shortestLength = INT_MAX;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QString text = model->cellText(row, messageColumn);
        if (text.size() > longestLength) {
            longestLength = text.size();
            longestRow = row;
        }
        if (text.size() < shortestLength) {
            shortestLength = text.size();
            shortestRow = row;
        }
        QVERIFY2(view->rowHeight(row) >= view->freshContentHeight(row),
                 qPrintable(QStringLiteral("row %1: applied=%2 content=%3")
                                .arg(row)
                                .arg(view->rowHeight(row))
                                .arg(view->freshContentHeight(row))));
    }
    QVERIFY(longestLength > shortestLength);
    QVERIFY(view->freshContentHeight(longestRow) > view->freshContentHeight(shortestRow));
    QVERIFY(view->rowHeight(longestRow) > view->rowHeight(shortestRow));
}

void TestMainWindowBehavior::rowHeightCoversTheTallestColumn()
{
    // Regression: the row height used to be derived from the message column
    // only. When another column needed more lines (a long Target with a short
    // message), the extra line was painted over the row separator - the
    // paragraph clip replaced the cell clip and let the text escape
    // (REQ-TABLE-04: nothing may bleed out of its cell).
    auto settings = makeSettings(QStringLiteral("row-height-columns"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    LogTableView *view = window.logTableView();
    LogTableModel *model = window.logModel();
    QVERIFY(view != nullptr);
    QVERIFY(model != nullptr);
    QVERIFY(view->fullContentMode());

    int targetColumn = -1;
    for (int column = 0; column < model->columnCount(); ++column) {
        if (model->columnKind(column) == ColumnKind::Target)
            targetColumn = column;
    }
    QVERIFY(targetColumn >= 0);

    // Squeeze the target column so its values need several lines while the
    // message column keeps fitting its own text (this combination used to
    // overflow: the row was sized from the message only).
    view->setColumnWidth(targetColumn, 60);

    const QFontMetrics metrics(view->font());
    const int twoLines = metrics.lineSpacing() * 2 + 6;

    // Wait for the deferred height pass, then every row has to fit its tallest
    // cell.
    QTRY_VERIFY([&] {
        for (int row = 0; row < model->rowCount(); ++row) {
            if (view->rowHeight(row) < view->freshContentHeight(row))
                return false;
        }
        return true;
    }());

    // The critical combination: a single line message with a target that wraps
    // into more than one line. The row used to be sized from the message only,
    // so the target was painted over the row separator.
    const int messageColumn = view->messageColumnIndex();
    QVERIFY(messageColumn >= 0);
    constexpr int kCellPadding = 14;      // LogItemDelegate::kHPadding * 2
    const int messageWidth = view->columnWidth(messageColumn) - kCellPadding;
    const int targetWidth = view->columnWidth(targetColumn) - kCellPadding;

    int checkedRows = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QString message = model->cellText(row, messageColumn);
        const QString target = model->cellText(row, targetColumn);
        if (message.contains(QLatin1Char('\n')) || target.contains(QLatin1Char('\n')))
            continue;                       // multi paragraph cells always need several lines
        if (metrics.horizontalAdvance(message) > messageWidth)
            continue;                       // the message alone needs several lines
        if (metrics.horizontalAdvance(target) <= targetWidth)
            continue;                       // the target fits on one line

        ++checkedRows;
        QVERIFY2(view->rowHeight(row) >= twoLines,
                 qPrintable(QStringLiteral("row %1: height=%2, message is one line but the "
                                           "target needs more")
                                .arg(row)
                                .arg(view->rowHeight(row))));
    }
    QVERIFY2(checkedRows > 0, "no row with a wrapping target and a single line message");

    // Painting guard: even when a cell is given a too short rectangle (a stale
    // height or a mid-layout pass), nothing may be painted below it.
    const QModelIndex index = model->index(0, targetColumn);
    QVERIFY(index.isValid());
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, view->columnWidth(targetColumn), twoLines / 2);
    option.palette = view->palette();
    option.state = QStyle::State_Enabled;

    QImage image(option.rect.width() + 40, option.rect.height() + 60,
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    view->itemDelegate()->paint(&painter, option, index);
    painter.end();

    for (int y = option.rect.bottom() + 1; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QVERIFY2(qAlpha(image.pixel(x, y)) == 0,
                     qPrintable(QStringLiteral("painted outside the cell at (%1,%2)")
                                    .arg(x)
                                    .arg(y)));
        }
    }
}

void TestMainWindowBehavior::uncheckedPaneIgnoresRowClicks()
{
    auto settings = makeSettings(QStringLiteral("click-off"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    clickRow(window, 3);

    QVERIFY(!window.isDetailPaneVisible());
}

void TestMainWindowBehavior::checkedSelectsFirstRow()
{
    auto settings = makeSettings(QStringLiteral("shown"), true);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.loadDemoData();

    QCOMPARE(window.currentRow(), 0);
    QVERIFY(window.isDetailPaneVisible());
}

void TestMainWindowBehavior::checkedPaneFollowsRowClicks()
{
    auto settings = makeSettings(QStringLiteral("follow"), true);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();
    QVERIFY(window.isDetailPaneVisible());

    clickRow(window, 2);

    QCOMPARE(window.currentRow(), 2);
    QVERIFY(window.isDetailPaneVisible());
}

void TestMainWindowBehavior::checkedKeepsPaneOnReload()
{
    auto settings = makeSettings(QStringLiteral("always-reload"), true);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.loadDemoData();
    QVERIFY(window.isDetailPaneVisible());

    window.loadDemoData();
    QVERIFY(window.isDetailPaneVisible());
    QCOMPARE(window.currentRow(), 0);
}

void TestMainWindowBehavior::reloadDoesNotOpenPaneWhenUnchecked()
{
    auto settings = makeSettings(QStringLiteral("regression"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.loadDemoData();
    clickRow(window, 1);
    QVERIFY(!window.isDetailPaneVisible());

    // Regression test: loading a document must not pop up the pane either.
    window.loadDemoData();
    QVERIFY(!window.isDetailPaneVisible());
    QCOMPARE(window.currentRow(), -1);
}

void TestMainWindowBehavior::uncheckingHidesPane()
{
    auto settings = makeSettings(QStringLiteral("uncheck"), true);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();
    QVERIFY(window.isDetailPaneVisible());

    settings->setShowDetailsPane(false);
    QVERIFY(!window.isDetailPaneVisible());

    // Switching it back on brings the pane (and the first row) back.
    settings->setShowDetailsPane(true);
    QVERIFY(window.isDetailPaneVisible());
    QCOMPARE(window.currentRow(), 0);
}

void TestMainWindowBehavior::detailsMessageWrapsWithoutHorizontalScrollBar()
{
    auto settings = makeSettings(QStringLiteral("wrap"), true);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();
    QVERIFY(window.isDetailPaneVisible());

    QPlainTextEdit *message = window.detailPane()->messageEdit();
    QVERIFY(message != nullptr);
    QCOMPARE(message->lineWrapMode(), QPlainTextEdit::WidgetWidth);
    QCOMPARE(message->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QCOMPARE(message->wordWrapMode(), QTextOption::WrapAtWordBoundaryOrAnywhere);

    // A very long single line must wrap to more than one visual line.
    const QString longLine(2000, QLatin1Char('x'));
    message->setPlainText(longLine);
    const int wrappedHeight = message->document()->size().height();
    QVERIFY2(wrappedHeight > 1, qPrintable(QString::number(wrappedHeight)));
}

void TestMainWindowBehavior::monitorAppendsRowsAndFollows()
{
    const QString livePath = QDir(m_dir.path()).filePath(QStringLiteral("live-ui.log"));
    {
        QFile file(livePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("2026-09-28 10:00:00 INFO first\n"
                   "2026-09-28 10:00:01 DEBUG second\n"
                   "2026-09-28 10:00:02 ERROR third\n");
    }

    auto settings = makeSettings(QStringLiteral("monitor"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.openPaths({livePath}, QString());
    QCOMPARE(window.logModel()->rowCount(), 3);
    QVERIFY(window.monitorAction()->isEnabled());     // file backed document
    QVERIFY(!window.monitorAction()->isChecked());

    // Demo data cannot be monitored, a real file can.
    window.monitorAction()->setChecked(true);
    QVERIFY(window.monitorAction()->isChecked());

    {
        QFile file(livePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text));
        file.write("2026-09-28 10:00:03 WARN fourth\n"
                   "2026-09-28 10:00:04 FATAL fifth\n");
    }
    QTRY_COMPARE_WITH_TIMEOUT(window.logModel()->rowCount(), 5, 5000);
    QVERIFY(window.logTableView()->isAtBottom());     // followed the tail

    // Switching monitoring off stops the updates.
    window.monitorAction()->setChecked(false);
    {
        QFile file(livePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text));
        file.write("2026-09-28 10:00:05 INFO sixth\n");
    }
    QTest::qWait(700);
    QCOMPARE(window.logModel()->rowCount(), 5);
}

void TestMainWindowBehavior::findHighlightsAndNavigates()
{
    auto settings = makeSettings(QStringLiteral("find"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    auto *findEdit = window.findChild<QLineEdit *>(QStringLiteral("findEdit"));
    QVERIFY(findEdit != nullptr);
    findEdit->setText(QStringLiteral("CONNECT"));

    // Typing does not scan the document; the pattern is applied on Enter
    // (REQ-FIND-07).
    QCOMPARE(window.logModel()->findMatchRowCount(), 0);
    QTest::keyClick(findEdit, Qt::Key_Return);

    QTRY_VERIFY(window.logModel()->findMatchRowCount() > 0);
    const int total = window.logModel()->findMatchRowCount();
    QVERIFY(total >= 1);
    QCOMPARE(window.currentRow(), window.logModel()->findRowAt(0));

    // F3 moves to the next matching row, Shift+F3 back to the first one.
    LogTableView *view = window.logTableView();
    QTest::keyClick(view, Qt::Key_F3);
    if (total > 1)
        QCOMPARE(window.currentRow(), window.logModel()->findRowAt(1 % total));
    QTest::keyClick(view, Qt::Key_F3, Qt::ShiftModifier);
    QCOMPARE(window.currentRow(), window.logModel()->findRowAt(0));

    // Clearing the pattern removes the highlight data again.
    findEdit->clear();
    QCOMPARE(window.logModel()->findMatchRowCount(), 0);
}

void TestMainWindowBehavior::filterAppliesOnEnterOnly()
{
    auto settings = makeSettings(QStringLiteral("filter-enter"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();
    QCOMPARE(window.logModel()->rowCount(), 10);

    auto *filterEdit = window.findChild<QLineEdit *>(QStringLiteral("filterEdit"));
    QVERIFY(filterEdit != nullptr);

    // Typing must not hide rows: the filter runs on Enter (or Apply).
    filterEdit->setText(QStringLiteral("plugin"));
    QTest::qWait(300);
    QCOMPARE(window.logModel()->rowCount(), 10);
    QVERIFY(!window.logModel()->isFiltered());

    QTest::keyClick(filterEdit, Qt::Key_Return);
    QTRY_COMPARE(window.logModel()->rowCount(), 2);
    QVERIFY(window.logModel()->isFiltered());

    // Clearing the box applies immediately: the rows come back.
    filterEdit->clear();
    QTRY_COMPARE(window.logModel()->rowCount(), 10);
    QVERIFY(!window.logModel()->isFiltered());
}

void TestMainWindowBehavior::invertedFilterShowsNonMatchingRows()
{
    // REQ-FILTER-09: the Invert check box flips the keyword filter - only the rows
    // *without* the pattern stay visible, and toggling it applies immediately.
    auto settings = makeSettings(QStringLiteral("invert-filter"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    auto *model = window.logModel();
    QVERIFY(model != nullptr);
    auto *filterEdit = window.findChild<QLineEdit *>(QStringLiteral("filterEdit"));
    auto *invert = window.findChild<QCheckBox *>(QStringLiteral("filterInvert"));
    QVERIFY(filterEdit != nullptr);
    QVERIFY(invert != nullptr);
    QVERIFY(!invert->isChecked());

    int messageColumn = -1;
    for (int column = 0; column < model->columnCount(); ++column) {
        if (model->columnKind(column) == ColumnKind::Message)
            messageColumn = column;
    }
    QVERIFY(messageColumn >= 0);

    const int totalRows = model->rowCount();
    QVERIFY(totalRows > 3);

    // Normal filter: only the rows containing the pattern are visible.
    filterEdit->setText(QStringLiteral("CONNECT"));
    QTest::keyClick(filterEdit, Qt::Key_Return);
    const int matchingRows = model->rowCount();
    QVERIFY(matchingRows > 0);
    QVERIFY(matchingRows < totalRows);
    for (int row = 0; row < matchingRows; ++row)
        QVERIFY(model->cellText(row, messageColumn).contains(QStringLiteral("CONNECT")));

    // Inverted: exactly the other rows, applied as soon as the box is checked.
    invert->setChecked(true);
    QTRY_COMPARE(model->rowCount(), totalRows - matchingRows);
    QVERIFY(model->isFiltered());
    for (int row = 0; row < model->rowCount(); ++row)
        QVERIFY(!model->cellText(row, messageColumn).contains(QStringLiteral("CONNECT")));

    // Unchecking goes back to the normal filter.
    invert->setChecked(false);
    QTRY_COMPARE(model->rowCount(), matchingRows);

    // Clear resets the inverted flag too (REQ-FILTER-06).
    invert->setChecked(true);
    QTRY_COMPARE(model->rowCount(), totalRows - matchingRows);
    auto *clear = window.findChild<QPushButton *>(QStringLiteral("filterClear"));
    QVERIFY(clear != nullptr);
    clear->click();
    QTRY_COMPARE(model->rowCount(), totalRows);
    QVERIFY(!invert->isChecked());
    QVERIFY(!model->isFiltered());
}

void TestMainWindowBehavior::levelFilterHidesNonMatchingRows()
{
    auto settings = makeSettings(QStringLiteral("filter"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();
    QCOMPARE(window.logModel()->rowCount(), 10);
    QVERIFY(!window.logModel()->isFiltered());

    // Keep only ERROR: uncheck every other *listed* level (the list is built from
    // the levels that occur in the document).
    const QList<QCheckBox *> levelChecks = window.findChildren<QCheckBox *>();
    int unchecked = 0;
    for (QCheckBox *check : levelChecks) {
        if (!check->objectName().startsWith(QLatin1String("levelCheck")))
            continue;
        if (check->objectName() != QLatin1String("levelCheckERROR")) {
            check->setChecked(false);
            ++unchecked;
        }
    }
    QVERIFY(unchecked >= 4);

    QTRY_COMPARE(window.logModel()->rowCount(), 2);
    QVERIFY(window.logModel()->isFiltered());

    // Re-checking every level restores the complete document.
    for (QCheckBox *check : window.findChildren<QCheckBox *>()) {
        if (check->objectName().startsWith(QLatin1String("levelCheck")))
            check->setChecked(true);
    }
    QTRY_COMPARE(window.logModel()->rowCount(), 10);
    QVERIFY(!window.logModel()->isFiltered());
}

void TestMainWindowBehavior::levelListFollowsTheDocument()
{
    auto settings = makeSettings(QStringLiteral("level-list"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const auto levelNames = [&window] {
        QStringList names;
        for (QCheckBox *check : window.findChildren<QCheckBox *>()) {
            if (check->objectName().startsWith(QLatin1String("levelCheck")))
                names << check->objectName().mid(10);
        }
        names.sort();
        return names;
    };

    // The demo data covers seven levels and has no OTHER entry.
    window.loadDemoData();
    QCOMPARE(levelNames(), QStringList({QStringLiteral("DEBUG"), QStringLiteral("ERROR"),
                                        QStringLiteral("FATAL"), QStringLiteral("INFO"),
                                        QStringLiteral("NOTICE"), QStringLiteral("TRACE"),
                                        QStringLiteral("WARN")}));

    // The frozen tracing sample only contains DEBUG, INFO and ERROR.
    const QString sample = QDir(QStringLiteral(LOGVIEWER_FROZEN_SAMPLE)).absolutePath();
    window.openPaths({sample}, QString());
    QTRY_COMPARE(levelNames(), QStringList({QStringLiteral("DEBUG"), QStringLiteral("ERROR"),
                                            QStringLiteral("INFO")}));

    // A level the user switched off stays off when the list is rebuilt.
    QCheckBox *debug = nullptr;
    for (QCheckBox *check : window.findChildren<QCheckBox *>()) {
        if (check->objectName() == QLatin1String("levelCheckDEBUG"))
            debug = check;
    }
    QVERIFY(debug != nullptr);
    debug->setChecked(false);
    QTRY_VERIFY(window.logModel()->isFiltered());
    QCOMPARE(window.logModel()->rowCount(), 8);      // 6 INFO + 2 ERROR

    window.loadDemoData();
    const QStringList names = levelNames();
    QCOMPARE(names.size(), 7);
    for (QCheckBox *check : window.findChildren<QCheckBox *>()) {
        if (check->objectName() == QLatin1String("levelCheckDEBUG"))
            QVERIFY2(!check->isChecked(), "the user's unchecking must survive a rebuild");
    }
}

void TestMainWindowBehavior::zebraStripesAndGridLines()
{
    // REQ-TABLE-09 / REQ-TABLE-10: alternate rows use a different background and
    // every cell is delimited by a grid line on its right and bottom edge.
    auto settings = makeSettings(QStringLiteral("table-colors"), true);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    auto *view = window.findChild<LogTableView *>();
    QVERIFY(view != nullptr);
    auto *delegate = qobject_cast<LogItemDelegate *>(view->itemDelegate());
    QVERIFY(delegate != nullptr);

    // Explicit, opaque colours: the platform style may hand out translucent
    // palette entries, which would make the pixel comparisons blend.
    const QColor base(0x20, 0x20, 0x20);
    const QColor alternateBase(0x2A, 0x2A, 0x2B);
    const QColor highlight(0x09, 0x47, 0x71);
    QPalette palette;
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternateBase);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::Text, QColor(0xD4, 0xD4, 0xD4));
    palette.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));

    const auto paintCell = [&](int row, const QStyle::State &state) {
        QImage image(120, 40, QImage::Format_ARGB32);
        image.fill(Qt::magenta);                 // shows anything left unpainted
        QStyleOptionViewItem option;
        option.rect = QRect(0, 0, 120, 40);
        option.state = state;
        option.palette = palette;
        option.widget = view;
        QPainter painter(&image);
        delegate->paint(&painter, option, window.logModel()->index(row, 0));   // Line column
        painter.end();
        return image;
    };

    const QImage even = paintCell(0, QStyle::State_Enabled);
    const QImage odd = paintCell(1, QStyle::State_Enabled);
    QCOMPARE(even.pixelColor(60, 20), base);
    QCOMPARE(odd.pixelColor(60, 20), alternateBase);

    // Grid lines: right and bottom edge differ from the cell fill and are the
    // same for both parities.
    const QColor separator = even.pixelColor(119, 20);
    QVERIFY(separator != base);
    QCOMPARE(odd.pixelColor(119, 20), separator);
    QCOMPARE(even.pixelColor(60, 39), separator);

    // The selection colour still wins over the zebra stripe.
    const QImage selected = paintCell(1, QStyle::State_Enabled | QStyle::State_Selected);
    QCOMPARE(selected.pixelColor(60, 20), highlight);
}

void TestMainWindowBehavior::filterPanelCollapsesToSingleRow()
{
    // REQ-UI-13: the "Search & Filter" group box collapses to its title row so
    // the log table gains the space. Collapsing only hides the form - already
    // applied find / filter conditions keep working.
    auto settings = makeSettings(QStringLiteral("collapse-panel"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    FilterPanel *panel = window.filterPanel();
    QVERIFY(panel != nullptr);
    QVERIFY(!panel->isCollapsed());
    const int expandedHeight = panel->height();
    QVERIFY(expandedHeight > 0);

    auto *toggle = panel->findChild<QToolButton *>(QStringLiteral("filterCollapseButton"));
    QVERIFY(toggle != nullptr);
    QVERIFY(toggle->isVisible());
    QCOMPARE(toggle->text(), QStringLiteral("▾"));

    // The toggle in the title row collapses the panel (form hidden) and the
    // freed space goes to the log table (nothing below the title row remains).
    QTest::mouseClick(toggle, Qt::LeftButton);
    QTRY_VERIFY(panel->isCollapsed());
    QCOMPARE(toggle->text(), QStringLiteral("▸"));
    QVERIFY(toggle->isVisible());
    QTRY_VERIFY(panel->height() < expandedHeight);
    const int collapsedHeight = panel->height();
    QVERIFY2(collapsedHeight * 2 <= expandedHeight,
             qPrintable(QStringLiteral("collapsed=%1 expanded=%2")
                            .arg(collapsedHeight)
                            .arg(expandedHeight)));
    const int tableHeightCollapsed = window.logTableView()->height();

    // Expanding restores the complete form and the table gives the space back.
    QTest::mouseClick(toggle, Qt::LeftButton);
    QTRY_VERIFY(!panel->isCollapsed());
    QCOMPARE(toggle->text(), QStringLiteral("▾"));
    QTRY_COMPARE(panel->height(), expandedHeight);
    QTRY_VERIFY(window.logTableView()->height() < tableHeightCollapsed);

    // A condition applied before collapsing keeps filtering while collapsed.
    auto *filterEdit = panel->findChild<QLineEdit *>(QStringLiteral("filterEdit"));
    QVERIFY(filterEdit != nullptr);
    filterEdit->setText(QStringLiteral("CONNECT"));
    QTest::keyClick(filterEdit, Qt::Key_Return);
    QTRY_VERIFY(window.logModel()->isFiltered());
    const int filteredRows = window.logModel()->rowCount();
    QVERIFY(filteredRows > 0);
    QVERIFY(filteredRows < window.logModel()->provider()->rowCount());

    panel->setCollapsed(true);
    QTRY_VERIFY(panel->isCollapsed());
    QCOMPARE(window.logModel()->rowCount(), filteredRows);

    // The API and the toggle button agree on the state.
    QSignalSpy spy(panel, &FilterPanel::collapsedChanged);
    panel->setCollapsed(false);
    QVERIFY(!panel->isCollapsed());
    QCOMPARE(spy.count(), 1);
}

void TestMainWindowBehavior::fullScreenCollapsesFilterPanel()
{
    // REQ-UI-14: Settings ▸ Full Screen (F11) collapses the search & filter
    // panel and maximises the window; Esc leaves full screen and restores the
    // collapse state the panel had before.
    auto settings = makeSettings(QStringLiteral("fullscreen"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    FilterPanel *panel = window.filterPanel();
    QAction *fullScreen = window.fullScreenAction();
    QVERIFY(panel != nullptr);
    QVERIFY(fullScreen != nullptr);
    QVERIFY(fullScreen->isCheckable());
    QCOMPARE(fullScreen->shortcut(), QKeySequence(Qt::Key_F11));

    // The action lives in the Settings menu (File is the first top level menu).
    const QList<QAction *> topLevel = window.menuBar()->actions();
    QVERIFY(topLevel.size() >= 2);
    QMenu *settingsMenu = topLevel.at(1)->menu();
    QVERIFY(settingsMenu != nullptr);
    QVERIFY(settingsMenu->actions().contains(fullScreen));

    QVERIFY(!window.isFullScreen());
    QVERIFY(!panel->isCollapsed());

    fullScreen->trigger();
    QTRY_VERIFY(window.isFullScreen());
    QVERIFY(fullScreen->isChecked());
    QVERIFY(panel->isCollapsed());

    // Esc leaves full screen and expands the panel again.
    QWidget *focus = window.focusWidget() ? window.focusWidget() : &window;
    QTest::keyClick(focus, Qt::Key_Escape);
    QTRY_VERIFY(!window.isFullScreen());
    QVERIFY(!fullScreen->isChecked());
    QVERIFY(!panel->isCollapsed());

    // F11 is the same toggle as the menu item.
    QTest::keyClick(window.focusWidget() ? window.focusWidget() : &window, Qt::Key_F11);
    QTRY_VERIFY(window.isFullScreen());
    QVERIFY(panel->isCollapsed());
    QTest::keyClick(window.focusWidget() ? window.focusWidget() : &window, Qt::Key_Escape);
    QTRY_VERIFY(!window.isFullScreen());
    QVERIFY(!panel->isCollapsed());

    // A panel the user collapsed before entering full screen stays collapsed
    // after leaving it.
    panel->setCollapsed(true);
    fullScreen->trigger();
    QTRY_VERIFY(window.isFullScreen());
    QVERIFY(panel->isCollapsed());
    QTest::keyClick(window.focusWidget() ? window.focusWidget() : &window, Qt::Key_Escape);
    QTRY_VERIFY(!window.isFullScreen());
    QVERIFY(panel->isCollapsed());

    // A window that was maximized before full screen comes back maximized
    // (REQ-UI-14). The filter panel state is restored either way.
    panel->setCollapsed(false);
    window.showMaximized();
    QTRY_VERIFY(window.isMaximized());
    fullScreen->trigger();
    QTRY_VERIFY(window.isFullScreen());
    QVERIFY(panel->isCollapsed());
    QTest::keyClick(window.focusWidget() ? window.focusWidget() : &window, Qt::Key_Escape);
    QTRY_VERIFY(!window.isFullScreen());
    QTRY_VERIFY(window.isMaximized());
    QVERIFY(!panel->isCollapsed());
}

void TestMainWindowBehavior::columnsMenuHidesColumns()
{
    // REQ-UI-12 / REQ-TABLE-11: a top level "Columns" menu between Settings and
    // About with one checkable item per column of the loaded document; unchecking
    // a column hides it, and the visibility is remembered per document.
    auto settings = makeSettings(QStringLiteral("columns-menu"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    auto *model = window.logModel();
    auto *view = window.logTableView();
    QVERIFY(model != nullptr);
    QVERIFY(view != nullptr);

    const auto text = [](const char *source) {
        return QCoreApplication::translate("MainWindow", source);
    };

    // Menu bar: File, Settings, Columns, About (REQ-UI-01 / REQ-UI-12).
    const QList<QAction *> topLevel = window.menuBar()->actions();
    QCOMPARE(topLevel.size(), 4);
    QCOMPARE(topLevel.at(0)->text(), text("&File"));
    QCOMPARE(topLevel.at(1)->text(), text("&Settings"));
    QCOMPARE(topLevel.at(2)->text(), text("&Columns"));
    QCOMPARE(topLevel.at(3)->text(), text("About"));
    QMenu *columnsMenu = topLevel.at(2)->menu();
    QVERIFY(columnsMenu != nullptr);

    const auto checkableItems = [columnsMenu] {
        QList<QAction *> items;
        for (QAction *action : columnsMenu->actions()) {
            if (action->isCheckable())
                items << action;
        }
        return items;
    };

    // One item per column of the document, all checked initially.
    QCOMPARE(checkableItems().size(), model->columnCount());
    for (int column = 0; column < model->columnCount(); ++column) {
        QCOMPARE(checkableItems().at(column)->text(),
                 model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
        QVERIFY(checkableItems().at(column)->isChecked());
    }

    int lineColumn = -1;
    int threadColumn = -1;
    int messageColumn = -1;
    for (int column = 0; column < model->columnCount(); ++column) {
        switch (model->columnKind(column)) {
        case ColumnKind::Line:    lineColumn = column; break;
        case ColumnKind::Thread:  threadColumn = column; break;
        case ColumnKind::Message: messageColumn = column; break;
        default: break;
        }
    }
    QVERIFY(lineColumn >= 0);
    QVERIFY(threadColumn >= 0);
    QVERIFY(messageColumn >= 0);

    // The Line column is always shown (REQ-PARSE-03): disabled item, and the
    // model refuses to hide it even when asked directly.
    QVERIFY(!checkableItems().at(lineColumn)->isEnabled());
    model->setColumnHidden(lineColumn, true);
    QVERIFY(!model->isColumnHidden(lineColumn));

    // Unchecking hides the column in the table, re-checking shows it again.
    QAction *threadAction = checkableItems().at(threadColumn);
    threadAction->trigger();
    QVERIFY(!threadAction->isChecked());
    QVERIFY(model->isColumnHidden(threadColumn));
    QVERIFY(view->isColumnHidden(threadColumn));
    threadAction->trigger();
    QVERIFY(threadAction->isChecked());
    QVERIFY(!model->isColumnHidden(threadColumn));
    QVERIFY(!view->isColumnHidden(threadColumn));

    // Find only searches visible columns: the demo messages contain "CONNECT",
    // so hiding the message column removes every match (the highlight could not
    // be seen anyway).
    auto *findEdit = window.findChild<QLineEdit *>(QStringLiteral("findEdit"));
    QVERIFY(findEdit != nullptr);
    findEdit->setText(QStringLiteral("CONNECT"));
    QTest::keyClick(findEdit, Qt::Key_Return);
    QTRY_VERIFY(model->findMatchRowCount() > 0);

    QAction *messageAction = checkableItems().at(messageColumn);
    messageAction->trigger();
    QVERIFY(model->isColumnHidden(messageColumn));
    QCOMPARE(model->findMatchRowCount(), 0);
    messageAction->trigger();
    QTRY_VERIFY(model->findMatchRowCount() > 0);

    // The selection is remembered per document (REQ-TABLE-11).
    threadAction->trigger();
    QVERIFY(model->isColumnHidden(threadColumn));
    window.loadDemoData();
    QVERIFY(model->isColumnHidden(threadColumn));
    QVERIFY(window.logTableView()->isColumnHidden(threadColumn));

    // "Show All Columns" brings every column back.
    QAction *showAll = nullptr;
    for (QAction *action : columnsMenu->actions()) {
        if (action->text() == text("Show All Columns"))
            showAll = action;
    }
    QVERIFY(showAll != nullptr);
    QVERIFY(showAll->isEnabled());
    showAll->trigger();
    QVERIFY(!model->isColumnHidden(threadColumn));
    QVERIFY(!window.logTableView()->isColumnHidden(threadColumn));
    QVERIFY(!showAll->isEnabled());          // nothing hidden any more
}

void TestMainWindowBehavior::autoFitColumnsFitsContent()
{
    // REQ-TABLE-05: "Auto-fit Columns" sizes the columns from their content - the
    // time stamp always keeps its full width (it must never wrap), the message
    // column keeps the remaining space and the other columns are scaled down
    // proportionally only when the table is too narrow for all of them.
    auto settings = makeSettings(QStringLiteral("auto-fit"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    auto *view = window.logTableView();
    auto *model = window.logModel();
    QVERIFY(view != nullptr);
    QVERIFY(model != nullptr);

    int timeColumn = -1;
    int levelColumn = -1;
    int targetColumn = -1;
    int messageColumn = -1;
    for (int column = 0; column < model->columnCount(); ++column) {
        switch (model->columnKind(column)) {
        case ColumnKind::Time:    timeColumn = column; break;
        case ColumnKind::Level:   levelColumn = column; break;
        case ColumnKind::Target:  targetColumn = column; break;
        case ColumnKind::Message: messageColumn = column; break;
        default: break;
        }
    }
    QVERIFY(timeColumn >= 0);
    QVERIFY(levelColumn >= 0);
    QVERIFY(targetColumn >= 0);
    QVERIFY(messageColumn >= 0);

    view->autoFitColumns();

    // The level chips are drawn with a bold font plus their own padding: the column
    // has to fit the widest level value of the *document*, not just the one Qt's
    // row window happens to sample (REQ-TABLE-05).
    QFont chipFont = view->font();
    chipFont.setBold(true);
    const QFontMetrics chipMetrics(chipFont);
    int widestChip = 0;
    for (int row = 0; row < model->rowCount(); ++row)
        widestChip = qMax(widestChip,
                          chipMetrics.horizontalAdvance(model->cellText(row, levelColumn)));
    QVERIFY(widestChip > 0);
    QVERIFY2(view->columnWidth(levelColumn) >= widestChip + 16 + 10,
             qPrintable(QStringLiteral("level=%1 chip=%2")
                            .arg(view->columnWidth(levelColumn))
                            .arg(widestChip)));

    // Every time stamp fits on one line (the column was sized for the widest one).
    const QFontMetrics metrics(view->font());
    int widestStamp = 0;
    for (int row = 0; row < model->rowCount(); ++row)
        widestStamp = qMax(widestStamp,
                           metrics.horizontalAdvance(model->cellText(row, timeColumn)));
    QVERIFY(widestStamp > 0);
    QVERIFY2(view->columnWidth(timeColumn) >= widestStamp + 10,
             qPrintable(QStringLiteral("time=%1 needs=%2")
                            .arg(view->columnWidth(timeColumn))
                            .arg(widestStamp)));
    // ... and no unnecessary blank space either (REQ-TABLE-05): the column is
    // content wide plus the cell padding and a small rounding guard.
    QVERIFY2(view->columnWidth(timeColumn) <= widestStamp + 24,
             qPrintable(QStringLiteral("time=%1 needs=%2")
                            .arg(view->columnWidth(timeColumn))
                            .arg(widestStamp)));

    // There is room left: the other columns keep their own content widths (they
    // differ, they are not all forced to one value) and the message column takes
    // the rest of the table.
    int widestTarget = 0;
    for (int row = 0; row < model->rowCount(); ++row)
        widestTarget = qMax(widestTarget,
                            metrics.horizontalAdvance(model->cellText(row, targetColumn)));
    QVERIFY(widestTarget > 0);
    QVERIFY(view->columnWidth(targetColumn) > 150);
    QVERIFY(view->columnWidth(targetColumn) >= widestTarget + 10);
    QVERIFY(view->columnWidth(targetColumn) <= widestTarget + 24);
    QVERIFY(view->columnWidth(levelColumn) < view->columnWidth(targetColumn));
    QVERIFY(view->columnWidth(messageColumn) >= view->viewport()->width() * 2 / 5);

    // The sections fill the viewport: no horizontal scrolling is needed.
    int sections = 0;
    for (int column = 0; column < model->columnCount(); ++column)
        sections += view->columnWidth(column);
    QVERIFY2(qAbs(sections - view->viewport()->width()) <= 2,
             qPrintable(QStringLiteral("sections=%1 viewport=%2")
                            .arg(sections)
                            .arg(view->viewport()->width())));

    // Narrow window: the remaining columns are scaled down (the wide layout
    // cannot be kept), while the time stamp still gets its full width and the
    // message column keeps its share.
    const int wideTarget = view->columnWidth(targetColumn);
    window.resize(1024, 700);
    QTRY_VERIFY(view->viewport()->width() < 1200);
    QTest::qWait(200);
    view->autoFitColumns();

    QVERIFY2(view->columnWidth(timeColumn) >= widestStamp + 10,
             qPrintable(QStringLiteral("narrow time=%1 needs=%2")
                            .arg(view->columnWidth(timeColumn))
                            .arg(widestStamp)));
    QVERIFY2(view->columnWidth(targetColumn) < wideTarget,
             qPrintable(QStringLiteral("narrow target=%1 wide=%2")
                            .arg(view->columnWidth(targetColumn))
                            .arg(wideTarget)));
    QVERIFY(view->columnWidth(messageColumn) >= view->viewport()->width() * 2 / 5 - 2);
    QVERIFY(view->columnWidth(messageColumn) > view->columnWidth(targetColumn));

    // The level chips are reserved like the time stamp: they stay complete even
    // when the table has to scale the other columns down (REQ-TABLE-05).
    QVERIFY2(view->columnWidth(levelColumn) >= widestChip + 16 + 10,
             qPrintable(QStringLiteral("narrow level=%1 chip=%2")
                            .arg(view->columnWidth(levelColumn))
                            .arg(widestChip)));

    sections = 0;
    for (int column = 0; column < model->columnCount(); ++column)
        sections += view->columnWidth(column);
    QVERIFY2(qAbs(sections - view->viewport()->width()) <= 4,
             qPrintable(QStringLiteral("narrow sections=%1 viewport=%2")
                            .arg(sections)
                            .arg(view->viewport()->width())));
}

void TestMainWindowBehavior::snippetColorsSurviveTruncation()
{
    // Regression test for "the table loses its syntax highlighting when the
    // details pane is shown": the narrower message column means more lines are
    // truncated, and the elided line must keep its format ranges (REQ-HL-05).
    const QVector<TokenSpan> tokens = SnippetTokenizer::tokenize(
        QStringLiteral("proxy config = {\"server\":\"23.105.206.76\",\"port\":9684}"));

    int keyOffset = -1;
    for (const TokenSpan &span : tokens) {
        if (span.kind == TokenKind::Key) {
            keyOffset = span.start;
            break;
        }
    }
    QVERIFY(keyOffset >= 0);

    // Format ranges as buildFormats() would produce them (paragraph relative).
    QVector<QTextLayout::FormatRange> paragraphFormats;
    for (const TokenSpan &span : tokens) {
        QTextCharFormat format;
        format.setForeground(QColor(0x00, 0x10, 0x80));
        paragraphFormats.append(QTextLayout::FormatRange{span.start, span.length, format});
    }

    // The visible line starts right before the key (like a wrapped row does) and
    // the line is elided to 20 characters.
    const int lineStart = qMax(0, keyOffset - 3);
    const QVector<QTextLayout::FormatRange> shifted =
        LogItemDelegate::shiftFormatsToElidedLine(paragraphFormats, lineStart, 20);

    QVERIFY(!shifted.isEmpty());
    bool keyCovered = false;
    for (const QTextLayout::FormatRange &range : shifted) {
        QVERIFY(range.start >= 0);
        QVERIFY(range.start + range.length <= 20);
        if (range.start <= keyOffset - lineStart
            && qMax(range.start, 0) <= qMax(keyOffset - lineStart, 0))
            keyCovered = true;
    }
    QVERIFY2(keyCovered, "the truncated line lost the token format ranges");

    // Ranges entirely behind the elided window are dropped.
    const QVector<QTextLayout::FormatRange> nothing =
        LogItemDelegate::shiftFormatsToElidedLine(paragraphFormats, 1000, 20);
    QVERIFY(nothing.isEmpty());
}

void TestMainWindowBehavior::droppingFilesOpensAndMerges()
{
    // Dropping log files onto the window opens (and merges) them (REQ-FILE-02).
    const QString first = QDir(m_dir.path()).filePath(QStringLiteral("drop-a.log"));
    const QString second = QDir(m_dir.path()).filePath(QStringLiteral("drop-b.log"));
    {
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("2026-09-28 10:00:00 INFO from a\n");
    }
    {
        QFile file(second);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("2026-09-28 10:00:01 INFO from b\n");
    }

    auto settings = makeSettings(QStringLiteral("drop"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.acceptDrops());

    QMimeData mimeData;
    mimeData.setUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)});

    const QPoint dropPoint = window.rect().center();
    QDragEnterEvent enterEvent(dropPoint, Qt::CopyAction, &mimeData, Qt::LeftButton,
                               Qt::NoModifier);
    QApplication::sendEvent(&window, &enterEvent);
    QVERIFY(enterEvent.isAccepted());

    QDropEvent dropEvent(QPointF(dropPoint), Qt::CopyAction, &mimeData, Qt::LeftButton,
                         Qt::NoModifier);
    QApplication::sendEvent(&window, &dropEvent);
    QVERIFY(dropEvent.isAccepted());

    QTRY_COMPARE(window.logModel()->rowCount(), 2);
    QVERIFY(window.logModel()->provider() != nullptr);
    QVERIFY(window.logModel()->provider()->documentInfo().multiFile);
    QCOMPARE(window.logModel()->provider()->sourceName(0), QStringLiteral("drop-a.log"));
    QCOMPARE(window.logModel()->provider()->sourceName(1), QStringLiteral("drop-b.log"));
}

void TestMainWindowBehavior::droppingAFolderIsIgnored()
{
    auto settings = makeSettings(QStringLiteral("drop-folder"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QMimeData mimeData;
    mimeData.setUrls({QUrl::fromLocalFile(m_dir.path())});
    const QPoint dropPoint = window.rect().center();

    QDragEnterEvent enterEvent(dropPoint, Qt::CopyAction, &mimeData, Qt::LeftButton,
                               Qt::NoModifier);
    QApplication::sendEvent(&window, &enterEvent);
    QVERIFY(!enterEvent.isAccepted());      // nothing openable in the payload

    QDropEvent dropEvent(QPointF(dropPoint), Qt::CopyAction, &mimeData, Qt::LeftButton,
                         Qt::NoModifier);
    QApplication::sendEvent(&window, &dropEvent);
    QCOMPARE(window.logModel()->rowCount(), 0);
}

void TestMainWindowBehavior::loadTimeLabelShowsOpenDuration()
{
    // REQ-UI-15: after a file is opened the bottom left corner of the status bar
    // shows the elapsed time. The label is a normal indicator (left of the
    // permanent file/statistics/monitor labels) and a document without a file
    // (demo data) clears it again.
    const QString path = QDir(m_dir.path()).filePath(QStringLiteral("load-time.log"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("2026-09-28 10:00:00 INFO first\n"
                   "2026-09-28 10:00:01 WARN second\n");
    }

    auto settings = makeSettings(QStringLiteral("load-time"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QLabel *label = window.loadTimeLabel();
    QVERIFY(label != nullptr);
    QCOMPARE(label->objectName(), QStringLiteral("loadTimeLabel"));
    QVERIFY(label->text().isEmpty());

    window.openPaths({path}, QString());
    QVERIFY(!label->text().isEmpty());

    // "Loaded in 0.42 s": translated prefix plus a formatted duration.
    const QString translated = QCoreApplication::translate("MainWindow", "Loaded in %1");
    const QString prefix = translated.left(translated.indexOf(QStringLiteral("%1")));
    QVERIFY2(label->text().startsWith(prefix), qPrintable(label->text()));
    QVERIFY(label->text().endsWith(QStringLiteral("s")));

    // The label is the left-most status widget: every permanent indicator
    // (file name, statistics, monitor) sits to its right.
    QStatusBar *bar = window.statusBar();
    const int loadX = label->mapTo(bar, QPoint(0, 0)).x();
    QVERIFY(loadX < bar->width() / 2);
    for (QLabel *other : bar->findChildren<QLabel *>()) {
        if (other == label)
            continue;
        const int otherX = other->mapTo(bar, QPoint(0, 0)).x();
        QVERIFY2(otherX > loadX, qPrintable(QStringLiteral("%1 at %2, load label at %3")
                                                .arg(other->text())
                                                .arg(otherX)
                                                .arg(loadX)));
    }

    // Closing the document removes the value (File ▸ Close).
    QAction *closeAction = nullptr;
    QMenu *fileMenu = window.menuBar()->actions().value(0)->menu();
    QVERIFY(fileMenu != nullptr);
    for (QAction *action : fileMenu->actions()) {
        if (action->shortcut() == QKeySequence(QKeySequence::Close)) {
            closeAction = action;
            break;
        }
    }
    QVERIFY(closeAction != nullptr);
    closeAction->trigger();
    QTRY_VERIFY(label->text().isEmpty());

    // A document without a file (demo data) does not show a load time either.
    window.loadDemoData();
    QVERIFY(label->text().isEmpty());
}

void TestMainWindowBehavior::headersFollowLanguageSwitch()
{
    // Regression: the column titles were resolved when the columns were built,
    // so switching the UI language retranslated every widget except the table
    // header (and the Columns menu). The titles are translated on demand now and
    // retranslateHeaders() refreshes the header (REQ-I18N-02).
    auto settings = makeSettings(QStringLiteral("headers-language"), false);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadDemoData();

    LogTableModel *model = window.logModel();
    QVERIFY(model != nullptr);

    const auto titleOf = [model](ColumnKind kind) {
        for (int column = 0; column < model->columnCount(); ++column) {
            if (model->columnKind(column) == kind)
                return model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        }
        return QString();
    };

    // The test starts without a translator installed: the source language is English.
    QCOMPARE(titleOf(ColumnKind::Message), QStringLiteral("Message"));

    translations.setLanguage(QStringLiteral("zh_CN"));
    const QString chinese = titleOf(ColumnKind::Message);
    if (chinese == QLatin1String("Message"))
        QSKIP("the Chinese translation catalogue is not available in this build");
    QCOMPARE(chinese, QStringLiteral("消息"));

    // ... and switching back restores the English titles.
    translations.setLanguage(QStringLiteral("en"));
    QCOMPARE(titleOf(ColumnKind::Message), QStringLiteral("Message"));
}

void TestMainWindowBehavior::resetAllKeepsDefaults()
{
    auto settings = makeSettings(QStringLiteral("reset"), true);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    settings->resetAll();

    QVERIFY(!settings->showDetailsPane());
    // Defaults hide the details pane, so the table shows the complete content.
    QVERIFY(window.isFullContentMode());
}

void TestMainWindowBehavior::associationMenuEntryExists()
{
    // REQ-UI-16: Settings ▸ File Association ▸ Associate .log Files. The check
    // mark mirrors the registry; the test executable is never registered, so it
    // must start unchecked and no registry write happens in this test.
    auto settings = makeSettings(QStringLiteral("association"), true);
    ThemeManager theme(settings.get());
    TranslationManager translations;
    MainWindow window(settings.get(), &theme, &translations);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QAction *action = window.associateAction();
    QVERIFY(action != nullptr);
    QVERIFY(action->isCheckable());
    QVERIFY(!action->text().isEmpty());
    QCOMPARE(action->isEnabled(), FileAssociation::isSupported());
    QVERIFY(!action->isChecked());

    // The action is reachable from the menu bar (inside the Settings menu).
    bool inMenuBar = false;
    for (QMenu *menu : window.menuBar()->findChildren<QMenu *>()) {
        if (menu->actions().contains(action))
            inMenuBar = true;
    }
    QVERIFY(inMenuBar);
    QVERIFY(!action->toolTip().isEmpty());
}

QTEST_MAIN(TestMainWindowBehavior)
#include "tst_mainwindow_behavior.moc"
