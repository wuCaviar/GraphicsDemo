#ifndef LINEITEM_H
#define LINEITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsLineItem>

class LineItem : public QGraphicsLineItem, public IGraphicsItem
{
public:
    enum { Type = UserType + LineItemType };

    explicit LineItem(QGraphicsItem *parent = nullptr);
    LineItem(const QLineF &line, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    ItemType itemType() const override { return LineItemType; }
    PropertyFlags propertyFlags() const override { return HasPen | HasRotation; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override { return pen(); }
    void setItemPen(const QPen &p) override { setPen(p); }
    QBrush itemBrush() const override { return Qt::NoBrush; }
    void setItemBrush(const QBrush &) override {}

    // 精确几何矩形 — 线段的包围矩形（不含画笔边距）
    QRectF geometryRect() const override { return QRectF(line().p1(), line().p2()).normalized(); }
    bool supportsGeometryRect() const override { return true; }

    void serialize(QDataStream &out) const override;
    void deserialize(QDataStream &in) override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
};

#endif // LINEITEM_H
