#ifndef FREEHANDITEM_H
#define FREEHANDITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsPathItem>

class FreehandItem : public QGraphicsPathItem, public IGraphicsItem
{
public:
    enum { Type = UserType + FreehandItemType };

    explicit FreehandItem(QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    ItemType itemType() const override { return FreehandItemType; }
    PropertyFlags propertyFlags() const override { return HasPen | HasRotation; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override { return pen(); }
    void setItemPen(const QPen &p) override { setPen(p); }
    QBrush itemBrush() const override { return Qt::NoBrush; }
    void setItemBrush(const QBrush &) override {}

    // 追加路径点（自由绘制时调用）
    void appendPoint(const QPointF &scenePos);

    void serialize(QDataStream &out) const override;
    void deserialize(QDataStream &in) override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
};

#endif // FREEHANDITEM_H
