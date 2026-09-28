#include "ui/LogHeaderView.h"

#include <QAbstractItemModel>
#include <QLinearGradient>
#include <QPainter>

namespace lv {

LogHeaderView::LogHeaderView(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
    setSectionsClickable(true);
    setHighlightSections(false);
    setStretchLastSection(false);
    setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_headerFont = font();
}

void LogHeaderView::setDarkTheme(bool dark)
{
    m_dark = dark;
    viewport()->update();
}

void LogHeaderView::setHeaderFont(const QFont &font)
{
    m_headerFont = font;
    m_headerFont.setBold(true);
    setFont(m_headerFont);
    resizeSections();
    viewport()->update();
}

QSize LogHeaderView::sizeHint() const
{
    const QFont effective = m_headerFont.family().isEmpty() ? font() : m_headerFont;
    const QFontMetrics metrics(effective);
    return QSize(120, metrics.height() + 14);
}

void LogHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid() || logicalIndex < 0)
        return;

    painter->save();

    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    if (m_dark) {
        gradient.setColorAt(0.0, QColor(0x3C, 0x3C, 0x3C));
        gradient.setColorAt(1.0, QColor(0x2D, 0x2D, 0x2D));
    } else {
        gradient.setColorAt(0.0, QColor(0xFA, 0xFA, 0xFA));
        gradient.setColorAt(1.0, QColor(0xE4, 0xE4, 0xE4));
    }
    painter->fillRect(rect, gradient);

    const QColor border = m_dark ? QColor(0x1F, 0x1F, 0x1F) : QColor(0xC8, 0xC8, 0xC8);
    painter->setPen(border);
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    const QColor separator = m_dark ? QColor(0x3A, 0x3A, 0x3E) : QColor(0xD9, 0xD9, 0xDD);
    painter->setPen(separator);
    painter->drawLine(rect.topRight(), rect.bottomRight());

    QFont font = m_headerFont.family().isEmpty() ? this->font() : m_headerFont;
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(m_dark ? QColor(0xE6, 0xE6, 0xE6) : QColor(0x1F, 0x1F, 0x1F));

    QString text;
    if (const QAbstractItemModel *itemModel = model())
        text = itemModel->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString();

    const QRect textRect = rect.adjusted(8, 0, -8, 0);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));

    painter->restore();
}

} // namespace lv
