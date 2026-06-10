#ifndef IMAGEITEM_H
#define IMAGEITEM_H

#include "IGraphicsItem.h"
#include "Utils/ImageUtils.h"
#include <QGraphicsPixmapItem>

class ImageItem
    : public QGraphicsPixmapItem
    , public IGraphicsItem
{
public:
    enum
    {
        Type = UserType + ImageItemType
    };

    explicit ImageItem(QGraphicsItem *parent = nullptr);
    ImageItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);
    ImageItem(const QPixmap &pixmap, const QSize &sourcePixelSize, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    ItemType itemType() const override { return ImageItemType; }
    PropertyFlags propertyFlags() const override { return HasImage; }
    bool isResizable() const override { return false; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override { return m_pen; }
    void setItemPen(const QPen &p) override
    {
        m_pen = p;
        update();
    }
    QBrush itemBrush() const override { return Qt::NoBrush; }
    void setItemBrush(const QBrush &) override { }

    // CMYK 颜色存储
    void setItemPenCmyk(double c, double m, double y, double k) override
    {
        m_penCmyk = { c, m, y, k, true };
    }
    bool hasPenCmyk() const override { return m_penCmyk.valid; }
    void penCmyk(double &c, double &m, double &y, double &k) const override
    {
        c = m_penCmyk.c;
        m = m_penCmyk.m;
        y = m_penCmyk.y;
        k = m_penCmyk.k;
    }
    void clearPenCmyk() override { m_penCmyk.valid = false; }

    QString filePath() const override { return m_filePath; }
    void setFilePath(const QString &path) override { m_filePath = path; }

    // 矩形尺寸（支持拖拽缩放）
    QRectF rect() const;
    void setRect(const QRectF &rect);

    // 重写 boundingRect 以返回 m_rect（若有）或原始图片尺寸
    QRectF boundingRect() const override;

    // 重写 shape 使命中区域匹配 m_rect（缩略图模式下 pixmap 尺寸 < m_rect）
    QPainterPath shape() const override;

    // 精确几何矩形 — 返回 m_rect 或原始图片包围
    QRectF geometryRect() const override;

    // CMYK 源标志（导入 CMYK TIFF 时设置，导出时用于延迟加载原始数据）
    bool isCmykSource() const { return m_isCmykSource; }
    void setCmykSource(bool v) { m_isCmykSource = v; }

    // 多页 TIFF 标志
    bool isMultiPageSource() const { return m_isMultiPage; }
    void setMultiPageSource(bool v) { m_isMultiPage = v; }

    // 原始图像元数据（导入时填充，导出时用于保持原始分辨率）
    QSize originalSize() const { return m_originalSize; }
    void setOriginalSize(const QSize &sz) { m_originalSize = sz; }
    int dpiX() const { return m_dpiX; }
    int dpiY() const { return m_dpiY; }
    void setDpi(int x, int y)
    {
        m_dpiX = x;
        m_dpiY = y;
    }

    void serialize(QDataStream &out) const override;
    bool deserialize(QDataStream &in) override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QPen m_pen;
    CmykColor m_penCmyk;
    QRectF m_rect; // 自定义包围矩形（由缩放手柄设置）
    QString m_filePath;
    QSize m_originalSize; // 原始图像尺寸（缩放前）
    int m_dpiX = 72;
    int m_dpiY = 72;
    bool m_isCmykSource = false;
    bool m_isMultiPage = false;
};

#endif // IMAGEITEM_H
