#include "ui/FilterPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QToolButton>

namespace lv {
namespace {

constexpr int kTimePresetMinutes[] = {5, 15, 60};

} // namespace

FilterPanel::FilterPanel(QWidget *parent)
    : QGroupBox(parent)
{
    buildLayout();
    retranslateUi();
}

void FilterPanel::buildLayout()
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(10, 14, 10, 10);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);

    // Row 0: Find (highlight only, never hides rows).
    m_findLabel = new QLabel;
    m_findLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_findEdit = new QLineEdit;
    m_findEdit->setObjectName(QStringLiteral("findEdit"));
    m_findEdit->setClearButtonEnabled(true);
    m_findEdit->setMinimumWidth(200);
    // A file dropped here must reach the main window (which opens it) instead of
    // being inserted as text.
    m_findEdit->setAcceptDrops(false);
    m_findMode = new QComboBox;
    m_findMode->setObjectName(QStringLiteral("findMode"));
    m_findCase = new QCheckBox(QStringLiteral("Aa"));
    m_findCase->setToolTip(QStringLiteral("Match case"));
    m_findPrevious = new QToolButton;
    m_findPrevious->setText(QStringLiteral("◀"));
    m_findNext = new QToolButton;
    m_findNext->setText(QStringLiteral("▶"));
    m_findCounter = new QLabel(QStringLiteral("0/0"));
    m_findCounter->setMinimumWidth(56);

    auto *findRow = new QHBoxLayout;
    findRow->setSpacing(6);
    findRow->addWidget(m_findEdit, 1);
    findRow->addWidget(m_findMode);
    findRow->addWidget(m_findCase);
    findRow->addWidget(m_findPrevious);
    findRow->addWidget(m_findNext);
    findRow->addWidget(m_findCounter);

    // Row 1: Filter (hides non matching rows).
    m_filterLabel = new QLabel;
    m_filterLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_filterEdit = new QLineEdit;
    m_filterEdit->setObjectName(QStringLiteral("filterEdit"));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setMinimumWidth(200);
    m_filterEdit->setAcceptDrops(false);
    m_filterMode = new QComboBox;
    m_filterMode->setObjectName(QStringLiteral("filterMode"));
    m_filterCase = new QCheckBox(QStringLiteral("Aa"));
    m_filterCase->setToolTip(QStringLiteral("Match case"));
    m_filterInvert = new QCheckBox;
    m_filterInvert->setObjectName(QStringLiteral("filterInvert"));
    m_filterApply = new QPushButton;
    m_filterClear = new QPushButton;
    m_filterClear->setObjectName(QStringLiteral("filterClear"));

    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(6);
    filterRow->addWidget(m_filterEdit, 1);
    filterRow->addWidget(m_filterMode);
    filterRow->addWidget(m_filterCase);
    filterRow->addWidget(m_filterInvert);
    filterRow->addWidget(m_filterApply);
    filterRow->addWidget(m_filterClear);

    // Row 2: level check boxes, built dynamically from the loaded document.
    m_levelsLabel = new QLabel;
    m_levelsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_levelsLayout = new QHBoxLayout;
    // The counters are gone (REQ-FILTER-02): keep a little more air between the
    // boxes so the row stays easy to read.
    m_levelsLayout->setSpacing(14);
    rebuildLevelChecks({});

    // Row 3: time range.
    m_timeLabel = new QLabel;
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_timeEnabled = new QCheckBox;
    m_timeFrom = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(-3600));
    m_timeFrom->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_timeFrom->setCalendarPopup(true);
    m_timeTo = new QDateTimeEdit(QDateTime::currentDateTime());
    m_timeTo->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_timeTo->setCalendarPopup(true);
    m_timeFrom->setEnabled(false);
    m_timeTo->setEnabled(false);

    auto *timeRow = new QHBoxLayout;
    timeRow->setSpacing(6);
    timeRow->addWidget(m_timeEnabled);
    timeRow->addWidget(m_timeFrom);
    timeRow->addWidget(new QLabel(QStringLiteral("→")));
    timeRow->addWidget(m_timeTo);
    for (int minutes : kTimePresetMinutes) {
        auto *button = new QPushButton;
        button->setProperty("presetMinutes", minutes);
        m_timePresets.append(button);
        timeRow->addWidget(button);
    }
    auto *todayButton = new QPushButton;
    todayButton->setProperty("presetMinutes", -1);
    m_timePresets.append(todayButton);
    timeRow->addWidget(todayButton);
    auto *allButton = new QPushButton;
    allButton->setProperty("presetMinutes", 0);
    m_timePresets.append(allButton);
    timeRow->addWidget(allButton);
    timeRow->addStretch(1);

    grid->addWidget(m_findLabel, 0, 0);
    grid->addLayout(findRow, 0, 1);
    grid->addWidget(m_filterLabel, 1, 0);
    grid->addLayout(filterRow, 1, 1);
    grid->addWidget(m_levelsLabel, 2, 0);
    grid->addLayout(m_levelsLayout, 2, 1);
    grid->addWidget(m_timeLabel, 3, 0);
    grid->addLayout(timeRow, 3, 1);
    grid->setColumnStretch(1, 1);

    // Connections -----------------------------------------------------------
    // Find / Filter are applied on Enter (or with the Apply / navigation
    // buttons): typing must not refresh the table while the user is still
    // entering a pattern. Clearing the box applies immediately because the
    // intent is unambiguous.
    const auto emitFind = [this] { emitFindChanged(); };
    connect(m_findEdit, &QLineEdit::returnPressed, this, &FilterPanel::emitFindChanged);
    connect(m_findEdit, &QLineEdit::textChanged, this, [this, emitFind](const QString &text) {
        if (text.isEmpty() && m_findWasActive) {
            m_findWasActive = false;
            emitFind();
        }
        if (!text.isEmpty())
            m_findWasActive = true;
    });
    connect(m_findMode, &QComboBox::currentIndexChanged, this, [emitFind](int) { emitFind(); });
    connect(m_findCase, &QCheckBox::toggled, this, [emitFind](bool) { emitFind(); });
    connect(m_findNext, &QToolButton::clicked, this, &FilterPanel::findNextRequested);
    connect(m_findPrevious, &QToolButton::clicked, this, &FilterPanel::findPreviousRequested);

    connect(m_filterEdit, &QLineEdit::returnPressed, this, &FilterPanel::emitFilterChanged);
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty() && m_filterWasActive) {
            m_filterWasActive = false;
            emitFilterChanged();
        }
        if (!text.isEmpty())
            m_filterWasActive = true;
    });
    connect(m_filterMode, &QComboBox::currentIndexChanged, this, [this](int) { emitFilterChanged(); });
    connect(m_filterCase, &QCheckBox::toggled, this, [this](bool) { emitFilterChanged(); });
    connect(m_filterInvert, &QCheckBox::toggled, this, [this](bool) { emitFilterChanged(); });
    connect(m_filterApply, &QPushButton::clicked, this, &FilterPanel::emitFilterChanged);
    connect(m_filterClear, &QPushButton::clicked, this, [this] {
        m_filterEdit->clear();
        m_filterWasActive = false;
        m_filterInvert->setChecked(false);
        setAllLevelsChecked(true);
        m_timeEnabled->setChecked(false);
        emitLevelsChanged();
        emitFilterChanged();
        emit timeRangeChanged();
    });

    for (int level = 0; level < m_levelChecks.size(); ++level) {
        connect(m_levelChecks.at(level), &QCheckBox::toggled, this, [this](bool) { emitLevelsChanged(); });
    }

    connect(m_timeEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        m_timeFrom->setEnabled(enabled);
        m_timeTo->setEnabled(enabled);
        emit timeRangeChanged();
    });
    connect(m_timeFrom, &QDateTimeEdit::dateTimeChanged, this, [this](const QDateTime &) {
        if (m_timeEnabled->isChecked())
            emit timeRangeChanged();
    });
    connect(m_timeTo, &QDateTimeEdit::dateTimeChanged, this, [this](const QDateTime &) {
        if (m_timeEnabled->isChecked())
            emit timeRangeChanged();
    });
    for (QPushButton *button : m_timePresets) {
        connect(button, &QPushButton::clicked, this, [this, button] {
            applyTimePreset(button->property("presetMinutes").toInt());
        });
    }
}

void FilterPanel::retranslateUi()
{
    setTitle(tr("Search && Filter"));
    m_findLabel->setText(tr("Find:"));
    m_filterLabel->setText(tr("Filter:"));
    m_levelsLabel->setText(tr("Levels:"));
    // The level names are language independent but the tool tips are not.
    for (int index = 0; index < m_levelChecks.size() && index < m_displayedLevels.size(); ++index) {
        const int level = m_displayedLevels.at(index);
        const int count = m_lastLevelCounts.size() == kLogLevelCount
            ? m_lastLevelCounts.at(level) : -1;
        m_levelChecks.at(index)->setToolTip(levelTooltip(count));
    }
    m_timeLabel->setText(tr("Time:"));

    m_findEdit->setPlaceholderText(tr("Highlight matches (rows stay visible)"));
    m_filterEdit->setPlaceholderText(tr("Hide rows that do not match"));
    m_findEdit->setToolTip(tr("Matches are highlighted; no row is hidden"));
    m_filterEdit->setToolTip(tr("Rows without a match are hidden"));

    const int findIndex = m_findMode->currentIndex();
    const int filterIndex = m_filterMode->currentIndex();
    m_findMode->clear();
    m_filterMode->clear();
    const QStringList modes = {tr("Whole word"), tr("Wildcard"), tr("Regular expression")};
    m_findMode->addItems(modes);
    m_filterMode->addItems(modes);
    m_findMode->setCurrentIndex(qBound(0, findIndex, modes.size() - 1));
    m_filterMode->setCurrentIndex(qBound(0, filterIndex, modes.size() - 1));

    m_findCase->setToolTip(tr("Match case"));
    m_filterCase->setToolTip(tr("Match case"));
    m_findPrevious->setToolTip(tr("Previous match (Shift+F3)"));
    m_findNext->setToolTip(tr("Next match (F3)"));
    m_findCounter->setToolTip(tr("Current match / total matches"));

    m_filterApply->setText(tr("Apply"));
    m_filterClear->setText(tr("Clear"));
    m_filterInvert->setText(tr("Invert"));
    m_filterInvert->setToolTip(tr("Show only rows that do not match the filter pattern"));

    for (int level = 0; level < m_levelChecks.size(); ++level)
        m_levelChecks.at(level)->setText(logLevelCanonicalName(static_cast<LogLevel>(m_displayedLevels.value(level))));

    const int presetCount = static_cast<int>(sizeof(kTimePresetMinutes) / sizeof(int));
    for (int i = 0; i < m_timePresets.size(); ++i) {
        if (i < presetCount) {
            if (kTimePresetMinutes[i] < 60)
                m_timePresets.at(i)->setText(tr("%1 min").arg(kTimePresetMinutes[i]));
            else
                m_timePresets.at(i)->setText(tr("1 hour"));
        } else if (i == presetCount) {
            m_timePresets.at(i)->setText(tr("Today"));
        } else {
            m_timePresets.at(i)->setText(tr("All"));
        }
    }
    m_timeEnabled->setToolTip(tr("Enable the time range filter"));
}

void FilterPanel::rebuildLevelChecks(const QVector<int> &counts)
{
    // Programmatic rebuild: must not re-enter the filter engine.
    const QSignalBlocker blocker(this);

    while (QLayoutItem *item = m_levelsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            // Detach first so findChild() cannot return a stale widget that is
            // only waiting for the event loop to delete it.
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
    m_levelChecks.clear();
    m_displayedLevels.clear();
    m_lastLevelCounts = counts;

    const bool known = counts.size() == kLogLevelCount;
    for (int level = 0; level < kLogLevelCount; ++level) {
        const int count = known ? counts.at(level) : -1;
        if (known && count <= 0)
            continue;          // this document has no entry of that level

        const QString name = logLevelCanonicalName(static_cast<LogLevel>(level));
        auto *check = new QCheckBox(name);
        check->setObjectName(QStringLiteral("levelCheck") + name);
        check->setChecked(!m_uncheckedLevels.contains(level));
        // The number of matching entries is not displayed any more; it stays
        // available in the tool tip.
        check->setToolTip(levelTooltip(count));

        connect(check, &QCheckBox::toggled, this, [this, level](bool checked) {
            if (checked)
                m_uncheckedLevels.remove(level);
            else
                m_uncheckedLevels.insert(level);
            emit levelsChanged(checkedLevelMask());
        });

        m_levelChecks.append(check);
        m_displayedLevels.append(level);
        m_levelsLayout->addWidget(check);
    }
    m_levelsLayout->addStretch(1);
}

QString FilterPanel::levelTooltip(int count) const
{
    if (count < 0)
        return tr("Show entries of this level");
    return tr("Show entries of this level (%1 entries)").arg(count);
}

void FilterPanel::setLevelCounts(const QVector<int> &counts)
{
    rebuildLevelChecks(counts);
}

void FilterPanel::updateLevelCounts(const QVector<int> &counts)
{
    // Live monitoring: when the level set is unchanged only the tool tips change
    // (a new level means the row has to be rebuilt).
    m_lastLevelCounts = counts;
    if (counts.size() == kLogLevelCount) {
        QVector<int> present;
        for (int level = 0; level < kLogLevelCount; ++level) {
            if (counts.at(level) > 0)
                present.append(level);
        }
        if (present == m_displayedLevels) {
            for (int index = 0; index < m_levelChecks.size() && index < present.size(); ++index)
                m_levelChecks.at(index)->setToolTip(levelTooltip(counts.at(present.at(index))));
            return;
        }
    }
    rebuildLevelChecks(counts);
}

bool FilterPanel::allAvailableLevelsChecked() const
{
    for (const QCheckBox *check : m_levelChecks) {
        if (!check->isChecked())
            return false;
    }
    return true;
}

int FilterPanel::checkedLevelMask() const
{
    int mask = 0;
    for (int index = 0; index < m_levelChecks.size(); ++index) {
        if (m_levelChecks.at(index)->isChecked() && index < m_displayedLevels.size())
            mask |= 1 << m_displayedLevels.at(index);
    }
    return mask;
}

QString FilterPanel::findText() const
{
    return m_findEdit->text();
}

FilterPanel::MatchMode FilterPanel::findMode() const
{
    return static_cast<MatchMode>(qMax(0, m_findMode->currentIndex()));
}

bool FilterPanel::findCaseSensitive() const
{
    return m_findCase->isChecked();
}

QString FilterPanel::filterText() const
{
    return m_filterEdit->text();
}

FilterPanel::MatchMode FilterPanel::filterMode() const
{
    return static_cast<MatchMode>(qMax(0, m_filterMode->currentIndex()));
}

bool FilterPanel::filterCaseSensitive() const
{
    return m_filterCase->isChecked();
}

bool FilterPanel::filterInverted() const
{
    return m_filterInvert->isChecked();
}

bool FilterPanel::timeRangeActive() const
{
    return m_timeEnabled->isChecked();
}

QDateTime FilterPanel::timeFrom() const
{
    return m_timeFrom->dateTime();
}

QDateTime FilterPanel::timeTo() const
{
    return m_timeTo->dateTime();
}

void FilterPanel::setFindCounter(int ordinal, int total)
{
    if (total <= 0)
        m_findCounter->setText(QStringLiteral("0/0"));
    else
        m_findCounter->setText(QStringLiteral("%1/%2").arg(qMax(0, ordinal)).arg(total));
}

void FilterPanel::emitFindChanged()
{
    emit findChanged(findText(), static_cast<int>(findMode()), findCaseSensitive());
}

void FilterPanel::emitFilterChanged()
{
    emit filterChanged(filterText(), static_cast<int>(filterMode()), filterCaseSensitive());
}

void FilterPanel::emitLevelsChanged()
{
    emit levelsChanged(checkedLevelMask());
}

void FilterPanel::applyTimePreset(int minutes)
{
    const QDateTime now = QDateTime::currentDateTime();
    if (minutes == 0) {
        m_timeEnabled->setChecked(false);
        m_timeFrom->setDateTime(now.addSecs(-3600));
        m_timeTo->setDateTime(now);
    } else if (minutes < 0) {
        m_timeFrom->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
        m_timeTo->setDateTime(now);
        m_timeEnabled->setChecked(true);
    } else {
        m_timeFrom->setDateTime(now.addSecs(-minutes * 60));
        m_timeTo->setDateTime(now);
        m_timeEnabled->setChecked(true);
    }
    emit timeRangeChanged();
}

void FilterPanel::setAllLevelsChecked(bool checked)
{
    const QSignalBlocker blocker(this);
    if (checked)
        m_uncheckedLevels.clear();
    for (QCheckBox *check : m_levelChecks)
        check->setChecked(checked);
}

} // namespace lv
