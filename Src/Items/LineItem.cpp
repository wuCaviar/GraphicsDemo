#include "LineItem.h"
#include <QPainter>

LineItem::LineItem(QGraphicsItem *parent) : QGraphicsLineItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

LineItem::LineItem(const QLineF &line, QGraphicsItem *parent)
    : QGraphicsLineItem(line, parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

QGraphicsItem *LineItem::cloneItem() const
{
    auto *item = new LineItem(line());
    item->setPen(pen());
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    item->setTransformOriginPoint(transformOriginPoint());
    return item;
}

void LineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->drawLine(line());
}

void LineItem::serialize(QDataStream &out) const
{
    out << line() << pen() << pos() << rotation();
}

bool LineItem::deserialize(QDataStream &in)
{
    QLineF l;
    QPen p;
    qreal rot;
    QPointF pos_;
    in >> l >> p >> pos_ >> rot;
    if (in.status() != QDataStream::Ok)
        return false;
    setLine(l);
    setPen(p);
    setPos(pos_);
    setRotation(rot);
    return true;
}
