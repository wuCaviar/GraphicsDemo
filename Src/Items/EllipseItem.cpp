#include "EllipseItem.h"
#include <QPainter>

EllipseItem::EllipseItem(QGraphicsItem *parent) : QGraphicsEllipseItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

EllipseItem::EllipseItem(const QRectF &rect, QGraphicsItem *parent)
    : QGraphicsEllipseItem(rect, parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

QGraphicsItem *EllipseItem::cloneItem() const
{
    auto *item = new EllipseItem(rect());
    item->setPen(pen());
    item->setBrush(brush());
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    return item;
}

void EllipseItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawEllipse(rect());
}

void EllipseItem::serialize(QDataStream &out) const
{
    out << rect() << pen() << brush() << pos() << rotation();
}

void EllipseItem::deserialize(QDataStream &in)
{
    QRectF r;
    QPen p;
    QBrush b;
    qreal rot;
    QPointF pos_;
    in >> r >> p >> b >> pos_ >> rot;
    setRect(r);
    setPen(p);
    setBrush(b);
    setPos(pos_);
    setRotation(rot);
}
