#pragma once

#include <QHeaderView>

namespace lv {

/// Table header with the "shadow" look required by spec.md REQ-TABLE-02:
/// bold text on a subtle vertical gradient with a thin bottom border.
class LogHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    explicit LogHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

    void setDarkTheme(bool dark);
    void setHeaderFont(const QFont &font);

    QSize sizeHint() const override;

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
    bool m_dark = false;
    QFont m_headerFont;
};

} // namespace lv
