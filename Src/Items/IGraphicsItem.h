#ifndef IGRAPHICSITEM_H
#define IGRAPHICSITEM_H

#include <QBrush>
#include <QDataStream>
#include <QFont>
#include <QGraphicsItem>
#include <QImage>
#include <QPen>

// 统一属性接口，所有自定义图元均实现此接口
class IGraphicsItem
{
public:
    // 序列化版本号 — 剪贴板数据头部，用于前向兼容
    static constexpr int kSerializationVersion = 1;

    enum ItemType {
        RectItemType       = 1,
        EllipseItemType    = 2,
        LineItemType       = 3,
        BezierCurveItemType = 4,
        TextItemType       = 5,
        ImageItemType      = 6,
        FreehandItemType   = 7,
    };

    enum PropertyFlag {
        HasPen      = 0x0001,
        HasBrush    = 0x0002,
        HasFont     = 0x0004,
        HasText     = 0x0008,
        HasImage    = 0x0010,
        HasRotation = 0x0020,
    };
    Q_DECLARE_FLAGS(PropertyFlags, PropertyFlag)

    virtual ~IGraphicsItem() = default;

    virtual ItemType itemType() const = 0;
    virtual PropertyFlags propertyFlags() const = 0;
    virtual QGraphicsItem *cloneItem() const = 0;

    // 通用属性
    virtual QPen itemPen() const = 0;
    virtual void setItemPen(const QPen &pen) = 0;
    virtual QBrush itemBrush() const = 0;
    virtual void setItemBrush(const QBrush &brush) = 0;

    // 文字属性
    virtual QString text() const { return {}; }
    virtual void setText(const QString &) {}
    virtual QFont itemFont() const { return {}; }
    virtual void setItemFont(const QFont &) {}

    // 图像属性
    virtual QImage itemImage() const { return {}; }
    virtual void setItemImage(const QImage &) {}
    virtual QString filePath() const { return {}; }
    virtual void setFilePath(const QString &) {}

    // 精确几何矩形（不含画笔边距），用于对齐/分布等精确计算
    // 默认实现返回空 QRectF 并报告不支持；子类应按需重写
    virtual QRectF geometryRect() const { return {}; }
    virtual bool supportsGeometryRect() const { return false; }

    // 设置几何矩形（用于属性面板的尺寸调整和 PropertyChangeCommand）
    // 默认空实现；支持 Geometry 属性的子类应重写
    virtual void setGeometryRect(const QRectF &) {}
    virtual bool supportsSetGeometryRect() const { return false; }

    // 序列化（剪贴板）
    virtual void serialize(QDataStream &out) const = 0;
    virtual bool deserialize(QDataStream &in) = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(IGraphicsItem::PropertyFlags)

// 从 QDataStream 反序列化创建图元
IGraphicsItem *createItemByType(IGraphicsItem::ItemType type);

// 过滤掉 CanvasItem 和 ResizeHandleItem，返回可操作的图元列表
// CanvasItem::Type = UserType + 100, ResizeHandleItem::Type = UserType + 200
inline QList<QGraphicsItem *> filterSelectableItems(const QList<QGraphicsItem *> &items)
{
    QList<QGraphicsItem *> result;
    result.reserve(items.size());
    for (auto *item : items) {
        int t = item->type();
        if (t != QGraphicsItem::UserType + 100 && t != QGraphicsItem::UserType + 200)
            result << item;
    }
    return result;
}

#endif // IGRAPHICSITEM_H
