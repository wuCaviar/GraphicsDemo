#ifndef BEZIERCURVEITEM_H
#define BEZIERCURVEITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsPathItem>

class BezierCurveItem : public QGraphicsPathItem, public IGraphicsItem
{
public:
    enum { Type = UserType + BezierCurveItemType };

    explicit BezierCurveItem(QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    ItemType itemType() const override { return BezierCurveItemType; }
    PropertyFlags propertyFlags() const override { return HasPen | HasRotation; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override { return pen(); }
    void setItemPen(const QPen &p) override { setPen(p); }
    QBrush itemBrush() const override { return Qt::NoBrush; }
    void setItemBrush(const QBrush &) override {}

    // 根据起点、两个控制点和终点构建三次贝塞尔曲线路径
    void setBezierCurve(const QPointF &start, const QPointF &cp1, const QPointF &cp2, const QPointF &end);

    void serialize(QDataStream &out) const override;
    bool deserialize(QDataStream &in) override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
};

#endif // BEZIERCURVEITEM_H
