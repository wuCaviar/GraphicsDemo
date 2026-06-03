#ifndef GRAPHICSITEMGROUP_H
#define GRAPHICSITEMGROUP_H

#include "IGraphicsItem.h"
#include <QGraphicsItemGroup>

class GraphicsItemGroup
    : public QGraphicsItemGroup
    , public IGraphicsItem
{
public:
    enum
    {
        Type = UserType + GroupItemType
    };

    explicit GraphicsItemGroup(QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    QRectF boundingRect() const override { return childrenBoundingRect(); }

    // 禁用 Qt 内置选中指示器（由 ResizeHandleItem 绘制选中框）
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // IGraphicsItem
    ItemType itemType() const override { return GroupItemType; }
    PropertyFlags propertyFlags() const override { return { }; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override { return { }; }
    void setItemPen(const QPen &) override { }
    QBrush itemBrush() const override { return { }; }
    void setItemBrush(const QBrush &) override { }

    QRectF geometryRect() const override;
    bool supportsGeometryRect() const override { return !containsImageItem(); }

    // 几何大小调整：纯基本图元组可缩放，含图片的组禁止缩放
    bool isResizable() const override { return !containsImageItem(); }
    void setGeometryRect(const QRectF &newRect) override;
    bool supportsSetGeometryRect() const override { return !containsImageItem(); }

    // 递归检查子图元中是否包含图片图元
    bool containsImageItem() const;

    void serialize(QDataStream &out) const override;
    bool deserialize(QDataStream &in) override;

    // 子图元管理
    void addChildFromScene(QGraphicsItem *child);
    QList<QGraphicsItem *> childGraphicsItems() const;
    void extractChildren();
};

#endif // GRAPHICSITEMGROUP_H
