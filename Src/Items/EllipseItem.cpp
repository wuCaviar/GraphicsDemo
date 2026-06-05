#include "EllipseItem.h"
#include "ColorUtils.h"

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
    item->setTransformOriginPoint(transformOriginPoint());
    if (m_penCmyk.valid)
        item->setItemPenCmyk(m_penCmyk.c, m_penCmyk.m, m_penCmyk.y, m_penCmyk.k);
    if (m_brushCmyk.valid)
        item->setItemBrushCmyk(m_brushCmyk.c, m_brushCmyk.m, m_brushCmyk.y, m_brushCmyk.k);
    if (!m_gradientCmyk.isEmpty())
        item->setGradientStopCmykMap(m_gradientCmyk);
    return item;
}

void EllipseItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->setBrush(ColorUtils::mapGradientBrushToRect(brush(), rect()));
    painter->drawEllipse(rect());
}

void EllipseItem::serialize(QDataStream &out) const
{
    out << rect() << pen() << brush() << pos() << rotation();
    writeCmykIfValid(out, m_penCmyk);
    writeCmykIfValid(out, m_brushCmyk);
    writeGradientCmykIfValid(out, m_gradientCmyk);
}

bool EllipseItem::deserialize(QDataStream &in)
{
    QRectF r;
    QPen p;
    QBrush b;
    qreal rot;
    QPointF pos_;
    in >> r >> p >> b >> pos_ >> rot;
    if (in.status() != QDataStream::Ok)
        return false;
    setRect(r);
    setPen(p);
    setBrush(b);
    setPos(pos_);
    setRotation(rot);
    if (!readCmykIfAvailable(in, in.device(), m_penCmyk))
        return false;
    if (!readCmykIfAvailable(in, in.device(), m_brushCmyk))
        return false;
    if (!readGradientCmykIfAvailable(in, in.device(), m_gradientCmyk))
        return false;
    return true;
}
