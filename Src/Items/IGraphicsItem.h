#ifndef IGRAPHICSITEM_H
#define IGRAPHICSITEM_H

#include <QBrush>
#include <QBuffer>
#include <QDataStream>
#include <QFont>
#include <QGraphicsItem>
#include <QImage>
#include <QMap>
#include <QPen>

#include "ColorTypes.h"

// 统一属性接口，所有自定义图元均实现此接口
class IGraphicsItem
{
public:
    // 序列化版本号 — 剪贴板数据头部，用于前向兼容
    // v2: 添加 CMYK 颜色数据序列化
    static constexpr int kSerializationVersion = 2;

    enum ItemType {
        RectItemType       = 1,
        EllipseItemType    = 2,
        LineItemType       = 3,
        BezierCurveItemType = 4,
        TextItemType       = 5,
        ImageItemType      = 6,
        FreehandItemType   = 7,
        GroupItemType      = 8,
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
    virtual QString filePath() const { return {}; }
    virtual void setFilePath(const QString &) {}

    // CMYK 颜色存储（可选，用于 TIFF 导出精确 CMYK 值）
    virtual void setItemPenCmyk(double c, double m, double y, double k) { Q_UNUSED(c); Q_UNUSED(m); Q_UNUSED(y); Q_UNUSED(k); }
    virtual bool hasPenCmyk() const { return false; }
    virtual void penCmyk(double &c, double &m, double &y, double &k) const { c = m = y = k = 100; }
    virtual void clearPenCmyk() {}

    virtual void setItemBrushCmyk(double c, double m, double y, double k) { Q_UNUSED(c); Q_UNUSED(m); Q_UNUSED(y); Q_UNUSED(k); }
    virtual bool hasBrushCmyk() const { return false; }
    virtual void brushCmyk(double &c, double &m, double &y, double &k) const { c = m = y = k = 100; }
    virtual void clearBrushCmyk() {}

    // 渐变 CMYK 颜色存储（按停止点位置索引，用于 TIFF 导出精确 CMYK 渐变）
    virtual void setGradientStopCmyk(double position, double c, double m, double y, double k) { Q_UNUSED(position); Q_UNUSED(c); Q_UNUSED(m); Q_UNUSED(y); Q_UNUSED(k); }
    virtual bool hasGradientStopCmyk(double position) const { Q_UNUSED(position); return false; }
    virtual void gradientStopCmyk(double position, double &c, double &m, double &y, double &k) const { Q_UNUSED(position); c = m = y = k = 0; }
    virtual void clearGradientCmyk() {}
    virtual QMap<double, CmykColor> gradientStopCmykMap() const { return {}; }
    virtual void setGradientStopCmykMap(const QMap<double, CmykColor> &) {}

    // 是否允许缩放手柄调整大小
    virtual bool isResizable() const { return true; }

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

// CMYK 序列化辅助函数
// 写入标记字节 + CMYK 数据（仅当 valid 时写入）
inline void writeCmykIfValid(QDataStream &out, const CmykColor &cmyk)
{
    if (cmyk.valid) {
        out << static_cast<quint8>(1) << cmyk.c << cmyk.m << cmyk.y << cmyk.k;
    }
}

// 读取 CMYK 数据（尝试读取标记字节，流末尾则跳过）
// 返回 true 表示成功读取或无需读取（旧格式），false 表示读取错误
inline bool readCmykIfAvailable(QDataStream &in, QIODevice *device, CmykColor &cmyk)
{
    if (device->atEnd())
        return true; // 旧格式，无 CMYK 数据
    quint8 marker;
    in >> marker;
    if (in.status() != QDataStream::Ok)
        return false;
    if (marker == 1) {
        in >> cmyk.c >> cmyk.m >> cmyk.y >> cmyk.k;
        if (in.status() != QDataStream::Ok)
            return false;
        cmyk.valid = true;
    }
    return true;
}

// 写入渐变停止点 CMYK 数据
inline void writeGradientCmykIfValid(QDataStream &out, const QMap<double, CmykColor> &map)
{
    if (map.isEmpty())
        return;
    out << static_cast<quint8>(1);
    out << static_cast<quint32>(map.size());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        out << it.key() << it.value().c << it.value().m << it.value().y << it.value().k;
    }
}

// 读取渐变停止点 CMYK 数据
inline bool readGradientCmykIfAvailable(QDataStream &in, QIODevice *device, QMap<double, CmykColor> &map)
{
    if (device->atEnd())
        return true;
    quint8 marker;
    in >> marker;
    if (in.status() != QDataStream::Ok)
        return false;
    if (marker == 1) {
        quint32 count;
        in >> count;
        for (quint32 i = 0; i < count; ++i) {
            double pos, c, m, y, k;
            in >> pos >> c >> m >> y >> k;
            if (in.status() != QDataStream::Ok)
                return false;
            map[pos] = {c, m, y, k, true};
        }
    }
    return true;
}

// 计算序列化后的总字节数（用于剪贴板长度前缀）
inline QByteArray serializeItemToBytes(IGraphicsItem *item)
{
    QByteArray binary;
    QDataStream out(&binary, QIODevice::WriteOnly);
    out << static_cast<int>(item->itemType());
    item->serialize(out);
    return binary;
}

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
        if (t != QGraphicsItem::UserType + 100 && t != QGraphicsItem::UserType + 200
            && !item->parentItem())
            result << item;
    }
    return result;
}

#endif // IGRAPHICSITEM_H
