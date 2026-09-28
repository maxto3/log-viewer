#include "ui/DetailPane.h"

#include "core/LogLevel.h"
#include "highlight/MessageTextHighlighter.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextOption>
#include <QVBoxLayout>

namespace lv {
namespace {

QLabel *fieldLabel(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QLabel *valueLabel(const QString &text)
{
    auto *label = new QLabel(text.isEmpty() ? QStringLiteral("—") : text);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    label->setWordWrap(true);
    return label;
}

} // namespace

DetailPane::DetailPane(QWidget *parent)
    : QGroupBox(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 14, 10, 10);
    layout->setSpacing(8);

    m_form = new QFormLayout;
    m_form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_form->setHorizontalSpacing(12);
    m_form->setVerticalSpacing(4);
    layout->addLayout(m_form);

    m_messageLabel = fieldLabel(QStringLiteral("Message"));
    layout->addWidget(m_messageLabel);

    m_message = new QPlainTextEdit;
    m_message->setReadOnly(true);
    // Long log lines wrap automatically; a horizontal scroll bar is never shown
    // (spec.md REQ-DETAIL-04).
    m_message->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_message->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_message->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_message->setAcceptDrops(false);   // drops are handled by the main window
    m_message->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_message->setMinimumHeight(90);
    layout->addWidget(m_message, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    m_copyMessage = new QPushButton;
    m_copyAll = new QPushButton;
    buttons->addWidget(m_copyMessage);
    buttons->addWidget(m_copyAll);
    layout->addLayout(buttons);

    connect(m_copyMessage, &QPushButton::clicked, this, &DetailPane::copyMessageToClipboard);
    connect(m_copyAll, &QPushButton::clicked, this, &DetailPane::copyAllFieldsToClipboard);

    m_highlighter = new MessageTextHighlighter(m_message->document());

    updateButtons();
    retranslateUi();
}

void DetailPane::setHighlightTheme(const HighlightTheme *theme)
{
    if (m_highlighter)
        m_highlighter->setTheme(theme);
}

void DetailPane::setFindHighlight(const Matcher &matcher, const QColor &background,
                                  const QColor &foreground)
{
    if (m_highlighter)
        m_highlighter->setFind(matcher, background, foreground);
}

void DetailPane::retranslateUi()
{
    setTitle(tr("Details"));
    m_messageLabel->setText(tr("Message"));
    m_copyMessage->setText(tr("Copy Message"));
    m_copyAll->setText(tr("Copy All Fields"));
    if (m_hasEntry)
        rebuildFields();
}

void DetailPane::setEntry(const LogEntry *entry, const QString &fileName)
{
    m_hasEntry = entry != nullptr;
    if (!entry) {
        clearEntry();
        return;
    }
    m_entry = *entry;
    m_fileName = fileName;
    m_message->setPlainText(m_entry.message);
    if (m_highlighter)
        m_highlighter->setContent(m_entry.message);
    rebuildFields();
    updateButtons();
}

void DetailPane::clearEntry()
{
    m_hasEntry = false;
    m_entry = LogEntry();
    m_fileName.clear();
    m_message->clear();
    while (m_form->rowCount() > 0)
        m_form->removeRow(0);
    updateButtons();
}

void DetailPane::setMessageFont(const QFont &font)
{
    QFont messageFont = font;
    if (messageFont.family().isEmpty())
        messageFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_message->setFont(messageFont);
}

void DetailPane::setDarkTheme(bool dark)
{
    m_dark = dark;
    // The message area uses the palette, so a repaint is enough for now;
    // dedicated code block colours are added with the syntax highlighter (M3).
    m_message->viewport()->update();
}

void DetailPane::updateButtons()
{
    m_copyMessage->setEnabled(m_hasEntry);
    m_copyAll->setEnabled(m_hasEntry);
}

void DetailPane::rebuildFields()
{
    while (m_form->rowCount() > 0)
        m_form->removeRow(0);

    const QString timeText = m_entry.time.isValid()
        ? m_entry.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("—");
    m_form->addRow(fieldLabel(tr("Time")), valueLabel(timeText));

    QString levelText = logLevelDisplayName(m_entry.level);
    if (!m_entry.rawLevel.isEmpty() && m_entry.rawLevel != levelText)
        levelText += QStringLiteral("  (%1)").arg(m_entry.rawLevel);
    m_form->addRow(fieldLabel(tr("Level")), valueLabel(levelText));

    if (!m_entry.thread.isEmpty())
        m_form->addRow(fieldLabel(tr("Thread")), valueLabel(m_entry.thread));
    if (!m_entry.target.isEmpty())
        m_form->addRow(fieldLabel(tr("Target")), valueLabel(m_entry.target));
    if (!m_entry.pid.isEmpty())
        m_form->addRow(fieldLabel(tr("PID")), valueLabel(m_entry.pid));
    if (!m_entry.host.isEmpty())
        m_form->addRow(fieldLabel(tr("Host")), valueLabel(m_entry.host));

    if (!m_fileName.isEmpty())
        m_form->addRow(fieldLabel(tr("File")), valueLabel(m_fileName));
    m_form->addRow(fieldLabel(tr("Line")), valueLabel(QString::number(m_entry.firstLine)));
    if (m_entry.physicalLines > 1)
        m_form->addRow(fieldLabel(tr("Lines")), valueLabel(QString::number(m_entry.physicalLines)));

    QStringList extraKeys = m_entry.extra.keys();
    extraKeys.sort(Qt::CaseInsensitive);
    for (const QString &key : extraKeys)
        m_form->addRow(fieldLabel(key), valueLabel(m_entry.extra.value(key)));
}

void DetailPane::copyMessageToClipboard()
{
    if (!m_hasEntry)
        return;
    QApplication::clipboard()->setText(m_entry.message);
}

void DetailPane::copyAllFieldsToClipboard()
{
    if (!m_hasEntry)
        return;
    QStringList lines;
    lines << QStringLiteral("Time: %1").arg(m_entry.time.isValid()
            ? m_entry.time.toString(Qt::ISODateWithMs) : QStringLiteral("-"));
    lines << QStringLiteral("Level: %1").arg(m_entry.rawLevel.isEmpty()
            ? logLevelCanonicalName(m_entry.level) : m_entry.rawLevel);
    if (!m_entry.thread.isEmpty())
        lines << QStringLiteral("Thread: %1").arg(m_entry.thread);
    if (!m_entry.target.isEmpty())
        lines << QStringLiteral("Target: %1").arg(m_entry.target);
    if (!m_entry.pid.isEmpty())
        lines << QStringLiteral("PID: %1").arg(m_entry.pid);
    if (!m_entry.host.isEmpty())
        lines << QStringLiteral("Host: %1").arg(m_entry.host);
    if (!m_fileName.isEmpty())
        lines << QStringLiteral("File: %1").arg(m_fileName);
    lines << QStringLiteral("Line: %1").arg(m_entry.firstLine);
    QStringList extraKeys = m_entry.extra.keys();
    extraKeys.sort(Qt::CaseInsensitive);
    for (const QString &key : extraKeys)
        lines << QStringLiteral("%1: %2").arg(key, m_entry.extra.value(key));
    lines << QStringLiteral("Message:") << m_entry.message;
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

} // namespace lv
