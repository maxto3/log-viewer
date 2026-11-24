#include "ui/EventLogChannelDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace lv {

EventLogChannelDialog::EventLogChannelDialog(const EventLogBackendPtr &backend, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select Windows Event Log Channel"));
    setModal(true);
    resize(560, 520);

    auto *layout = new QVBoxLayout(this);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter channels"));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    layout->addWidget(m_buttons);

    QString error;
    const QVector<EventLogChannel> channels =
        backend ? backend->channels(&error) : QVector<EventLogChannel>{};
    if (!error.isEmpty() || channels.isEmpty()) {
        m_status->setText(error.isEmpty() ? tr("No event log channels were found.") : error);
        m_status->show();
    } else {
        m_list->addItems([&channels] {
            QStringList names;
            names.reserve(channels.size());
            for (const EventLogChannel &channel : channels)
                names << channel.name;
            return names;
        }());
    }

    connect(m_filter, &QLineEdit::textChanged, this, &EventLogChannelDialog::applyFilter);
    connect(m_list, &QListWidget::itemSelectionChanged, this,
            &EventLogChannelDialog::updateOpenState);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { accept(); });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &EventLogChannelDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &EventLogChannelDialog::reject);

    updateOpenState();
}

QString EventLogChannelDialog::selectedChannel() const
{
    const QListWidgetItem *item = m_list->currentItem();
    return item ? item->text() : QString();
}

void EventLogChannelDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        const bool matches = needle.isEmpty()
            || item->text().contains(needle, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }

    if (m_list->currentItem() && m_list->currentItem()->isHidden())
        m_list->setCurrentItem(nullptr);
    updateOpenState();
}

void EventLogChannelDialog::updateOpenState()
{
    if (QPushButton *open = m_buttons->button(QDialogButtonBox::Open))
        open->setEnabled(!selectedChannel().isEmpty());
}

} // namespace lv
