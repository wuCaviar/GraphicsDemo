#include "BezierCurveItem.h"
#include <QPainter>

BezierCurveItem::BezierCurveItem(QGraphicsItem *parent) : QGraphicsPathItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

QGraphicsItem *BezierCurveItem::cloneItem() const
{
    auto *item = new BezierCurveItem();
    item->setPath(path());
    item->setPen(pen());
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    item->setTransformOriginPoint(transformOriginPoint());
    return item;
}

void BezierCurveItem::setBezierCurve(const QPointF &start, const QPointF &cp1, const QPointF &cp2, const QPointF &end)
{
    QPainterPath p;
    p.moveTo(start);
    p.cubicTo(cp1, cp2, end);
    setPath(p);
}

void BezierCurveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->drawPath(path());
}

void BezierCurveItem::serialize(QDataStream &out) const
{
    out << path() << pen() << pos() << rotation();
}

bool BezierCurveItem::deserialize(QDataStream &in)
{
    QPainterPath p;
    QPen pen_;
    qreal rot;
    QPointF pos_;
    in >> p >> pen_ >> pos_ >> rot;
    if (in.status() != QDataStream::Ok)
        return false;
    setPath(p);
    setPen(pen_);
    setPos(pos_);
    setRotation(rot);
    return true;
}
