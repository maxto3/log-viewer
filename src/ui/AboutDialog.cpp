#include "ui/AboutDialog.h"

#include "platform/PlatformInfo.h"

#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace lv {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setModal(true);
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(10);

    m_title = new QLabel;
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.4);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    layout->addWidget(m_title);

    m_version = new QLabel;
    layout->addWidget(m_version);

    m_build = new QLabel;
    m_build->setWordWrap(true);
    layout->addWidget(m_build);

    m_description = new QLabel;
    m_description->setWordWrap(true);
    layout->addWidget(m_description);

    m_license = new QLabel;
    m_license->setWordWrap(true);
    m_license->setTextFormat(Qt::RichText);
    layout->addWidget(m_license);

    layout->addStretch(1);

    m_close = new QPushButton;
    m_close->setDefault(true);
    connect(m_close, &QPushButton::clicked, this, &QDialog::accept);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(m_close);
    layout->addLayout(buttons);

    retranslateUi();
}

void AboutDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void AboutDialog::retranslateUi()
{
    setWindowTitle(tr("About Log Viewer"));
    m_title->setText(tr("Log Viewer"));
    m_version->setText(tr("Version %1").arg(QApplication::applicationVersion()));
    m_build->setText(tr("Build: %1").arg(PlatformInfo::buildInfo()));
    m_description->setText(
        tr("A fast log file viewer for Windows and Linux with table based reading, "
           "filtering by level, time and keyword, syntax highlighting for embedded "
           "JSON, XML and YAML snippets, and live monitoring (tail -f)."));
    m_license->setText(
        tr("Licensed under the MIT License. Built with Qt %1, used under the terms of the "
           "GNU Lesser General Public License v3.")
            .arg(QString::fromLatin1(qVersion())));
    m_close->setText(tr("Close"));
}

} // namespace lv
