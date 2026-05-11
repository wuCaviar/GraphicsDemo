#include "FreehandItem.h"
#include <QPainter>

FreehandItem::FreehandItem(QGraphicsItem *parent) : QGraphicsPathItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

QGraphicsItem *FreehandItem::cloneItem() const
{
    auto *item = new FreehandItem();
    item->setPath(path());
    item->setPen(pen());
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    item->setTransformOriginPoint(transformOriginPoint());
    return item;
}

void FreehandItem::appendPoint(const QPointF &scenePos)
{
    // 将场景坐标转换为图元本地坐标后追加
    QPointF localPos = mapFromScene(scenePos);
    QPainterPath p = path();
    if (p.elementCount() == 0)
        p.moveTo(localPos);
    else
        p.lineTo(localPos);
    setPath(p);
}

void FreehandItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->drawPath(path());
}

void FreehandItem::serialize(QDataStream &out) const
{
    out << path() << pen() << pos() << rotation();
}

bool FreehandItem::deserialize(QDataStream &in)
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
