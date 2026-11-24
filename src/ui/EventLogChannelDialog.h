#pragma once

#include "core/IEventLogBackend.h"

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;

namespace lv {

/// Searchable list of every Windows event log channel (REQ-EVT-02).
class EventLogChannelDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EventLogChannelDialog(const EventLogBackendPtr &backend, QWidget *parent = nullptr);

    /// Name of the selected channel, empty when nothing is selected.
    QString selectedChannel() const;

private:
    void applyFilter(const QString &text);
    void updateOpenState();

    QLineEdit *m_filter = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};

} // namespace lv
