#ifndef GRAPHICSITEMGROUP_H
#define GRAPHICSITEMGROUP_H

#include "IGraphicsItem.h"
#include <QGraphicsItemGroup>

class GraphicsItemGroup : public QGraphicsItemGroup, public IGraphicsItem
{
public:
    enum { Type = UserType + GroupItemType };

    explicit GraphicsItemGroup(QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    // IGraphicsItem
    ItemType itemType() const override { return GroupItemType; }
    PropertyFlags propertyFlags() const override { return HasRotation; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override { return {}; }
    void setItemPen(const QPen &) override {}
    QBrush itemBrush() const override { return {}; }
    void setItemBrush(const QBrush &) override {}

    QRectF geometryRect() const override;
    bool supportsGeometryRect() const override { return true; }

    void serialize(QDataStream &out) const override;
    bool deserialize(QDataStream &in) override;

    // 子图元管理
    void addChildFromScene(QGraphicsItem *child);
    QList<QGraphicsItem *> childGraphicsItems() const;
    void extractChildren();
};

#endif // GRAPHICSITEMGROUP_H
