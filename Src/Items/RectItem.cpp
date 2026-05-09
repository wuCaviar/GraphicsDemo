#include "RectItem.h"
#include "ColorUtils.h"

#include <QPainter>

RectItem::RectItem(QGraphicsItem *parent) : QGraphicsRectItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

RectItem::RectItem(const QRectF &rect, QGraphicsItem *parent)
    : QGraphicsRectItem(rect, parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

QGraphicsItem *RectItem::cloneItem() const
{
    auto *item = new RectItem(rect());
    item->setPen(pen());
    item->setBrush(brush());
    item->setCornerRadius(m_cornerRadius);
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    return item;
}

qreal RectItem::cornerRadius() const { return m_cornerRadius; }

void RectItem::setCornerRadius(qreal r)
{
    if (qFuzzyCompare(m_cornerRadius, r))
        return;
    m_cornerRadius = r;
    update();
}

void RectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->setBrush(ColorUtils::mapGradientBrushToRect(brush(), rect()));

    if (m_cornerRadius > 0)
        painter->drawRoundedRect(rect(), m_cornerRadius, m_cornerRadius);
    else
        painter->drawRect(rect());
}

void RectItem::serialize(QDataStream &out) const
{
    out << rect() << pen() << brush() << m_cornerRadius << pos() << rotation();
}

void RectItem::deserialize(QDataStream &in)
{
    QRectF r;
    QPen p;
    QBrush b;
    qreal cr, rot;
    QPointF pos_;
    in >> r >> p >> b >> cr >> pos_ >> rot;
    setRect(r);
    setPen(p);
    setBrush(b);
    m_cornerRadius = cr;
    setPos(pos_);
    setRotation(rot);
}
