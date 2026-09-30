#include "ui/StatusWarningLabel.h"

#include <QResizeEvent>
#include <QShowEvent>

namespace lv {

StatusWarningLabel::StatusWarningLabel(QWidget *parent)
    : QLabel(parent)
{
    setObjectName(QStringLiteral("statusWarning"));
    hide();
}

void StatusWarningLabel::setFullText(const QString &text)
{
    m_fullText = text;
    setToolTip(text);
    updateElidedText();
}

void StatusWarningLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    updateElidedText();
}

void StatusWarningLabel::showEvent(QShowEvent *event)
{
    QLabel::showEvent(event);
    updateElidedText();
}

void StatusWarningLabel::updateElidedText()
{
    setText(fontMetrics().elidedText(m_fullText, Qt::ElideMiddle, qMax(0, width())));
}

} // namespace lv
