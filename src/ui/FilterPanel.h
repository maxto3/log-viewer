#pragma once

#include "core/LogLevel.h"

#include <QGroupBox>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace lv {

/// "Search & Filter" group box (spec.md REQ-FIND / REQ-FILTER).
///
/// The panel only collects user input; the matching engine is wired up in a
/// later milestone, so the signals below are the integration point.
class FilterPanel : public QGroupBox
{
    Q_OBJECT

public:
    enum class MatchMode { WholeWord = 0, Wildcard = 1, RegularExpression = 2 };

    explicit FilterPanel(QWidget *parent = nullptr);

    /// Rebuilds the level check boxes from the level histogram of the loaded
    /// document: only the levels that actually occur are listed. The user's
    /// unchecked levels are remembered across documents.
    void setLevelCounts(const QVector<int> &counts);
    /// Same, but without widget churn when the set of levels did not change
    /// (used by live monitoring).
    void updateLevelCounts(const QVector<int> &counts);
    /// Bit mask of the currently checked levels (bit i == logLevelIndex i).
    int checkedLevelMask() const;
    /// True when every listed level is checked, i.e. the level filter has no
    /// effect and can be skipped entirely.
    bool allAvailableLevelsChecked() const;

    QString findText() const;
    MatchMode findMode() const;
    bool findCaseSensitive() const;

    QString filterText() const;
    MatchMode filterMode() const;
    bool filterCaseSensitive() const;
    /// True when the filter is inverted: only rows *without* a match stay visible
    /// (REQ-FILTER-09).
    bool filterInverted() const;

    bool timeRangeActive() const;
    QDateTime timeFrom() const;
    QDateTime timeTo() const;

    /// Updates the "current/total" find counter.
    void setFindCounter(int ordinal, int total);

public slots:
    void retranslateUi();

signals:
    void findChanged(const QString &text, int mode, bool caseSensitive);
    void filterChanged(const QString &text, int mode, bool caseSensitive);
    void levelsChanged(int levelMask);
    void timeRangeChanged();
    void findNextRequested();
    void findPreviousRequested();

private:
    void buildLayout();
    void emitFindChanged();
    void emitFilterChanged();
    void emitLevelsChanged();
    /// (Re)creates the check boxes for \a counts (empty = counts unknown: all
    /// levels are listed with an unknown counter).
    void rebuildLevelChecks(const QVector<int> &counts);
    /// Tooltip of a level check box ("Show entries of this level"); \a count < 0
    /// means the histogram is unknown (no document / not computed).
    QString levelTooltip(int count) const;
    void applyTimePreset(int minutes);
    void setAllLevelsChecked(bool checked);

    QLineEdit *m_findEdit = nullptr;
    QComboBox *m_findMode = nullptr;
    QCheckBox *m_findCase = nullptr;
    QToolButton *m_findPrevious = nullptr;
    QToolButton *m_findNext = nullptr;
    QLabel *m_findCounter = nullptr;

    QLineEdit *m_filterEdit = nullptr;
    QComboBox *m_filterMode = nullptr;
    QCheckBox *m_filterCase = nullptr;
    QCheckBox *m_filterInvert = nullptr;
    QPushButton *m_filterApply = nullptr;
    QPushButton *m_filterClear = nullptr;

    QVector<QCheckBox *> m_levelChecks;
    /// Level indices currently shown, parallel to m_levelChecks.
    QVector<int> m_displayedLevels;
    /// Last histogram handed to setLevelCounts() / updateLevelCounts(): empty
    /// when unknown, otherwise kLogLevelCount entries. Only used for the tool
    /// tips (the numbers themselves are no longer displayed).
    QVector<int> m_lastLevelCounts;
    /// Levels the user switched off (remembered across documents).
    QSet<int> m_uncheckedLevels;
    QHBoxLayout *m_levelsLayout = nullptr;

    QCheckBox *m_timeEnabled = nullptr;
    QDateTimeEdit *m_timeFrom = nullptr;
    QDateTimeEdit *m_timeTo = nullptr;
    QVector<QPushButton *> m_timePresets;

    QLabel *m_findLabel = nullptr;
    QLabel *m_filterLabel = nullptr;
    QLabel *m_levelsLabel = nullptr;
    QLabel *m_timeLabel = nullptr;

    /// True while the box contains text that was already applied; used to apply
    /// an empty box immediately when the user clears it.
    bool m_findWasActive = false;
    bool m_filterWasActive = false;
};

} // namespace lv
