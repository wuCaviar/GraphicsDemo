#ifndef IMAGEITEM_H
#define IMAGEITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsPixmapItem>

class ImageItem : public QGraphicsPixmapItem, public IGraphicsItem
{
public:
    enum { Type = UserType + ImageItemType };

    explicit ImageItem(QGraphicsItem *parent = nullptr);
    ImageItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    ItemType itemType() const override { return ImageItemType; }
    PropertyFlags propertyFlags() const override { return HasPen | HasImage | HasRotation; }
    QGraphicsItem *cloneItem() const override;

    QPen itemPen() const override { return m_pen; }
    void setItemPen(const QPen &p) override { m_pen = p; update(); }
    QBrush itemBrush() const override { return Qt::NoBrush; }
    void setItemBrush(const QBrush &) override {}

    // CMYK 颜色存储
    void setItemPenCmyk(double c, double m, double y, double k) override { m_penCmyk = {c, m, y, k, true}; }
    bool hasPenCmyk() const override { return m_penCmyk.valid; }
    void penCmyk(double &c, double &m, double &y, double &k) const override { c = m_penCmyk.c; m = m_penCmyk.m; y = m_penCmyk.y; k = m_penCmyk.k; }
    void clearPenCmyk() override { m_penCmyk.valid = false; }

    QImage itemImage() const override;
    void setItemImage(const QImage &img) override;
    QString filePath() const override { return m_filePath; }
    void setFilePath(const QString &path) override { m_filePath = path; }

    // 矩形尺寸（支持拖拽缩放）
    QRectF rect() const;
    void setRect(const QRectF &rect);

    // 重写 boundingRect 以返回 m_rect（若有）或原始图片尺寸
    QRectF boundingRect() const override;

    // 精确几何矩形 — 返回 m_rect 或原始图片包围
    QRectF geometryRect() const override;
    bool supportsGeometryRect() const override { return true; }
    void setGeometryRect(const QRectF &rect) override { setRect(rect); }
    bool supportsSetGeometryRect() const override { return true; }

    // 原始 TIFF 数据，用于无损导出
    QByteArray rawTiffData() const { return m_rawTiffData; }
    void setRawTiffData(const QByteArray &data) { m_rawTiffData = data; }

    // CMYK 源数据（导入 CMYK TIFF 时保留原始像素）
    bool isCmykSource() const { return m_isCmykSource; }
    void setCmykSourceData(const QByteArray &data, int w, int h);
    const QByteArray &rawCmykPixels() const { return m_rawCmykPixels; }
    int cmykSourceWidth() const { return m_cmykWidth; }
    int cmykSourceHeight() const { return m_cmykHeight; }

    void serialize(QDataStream &out) const override;
    bool deserialize(QDataStream &in) override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

private:
    QPen m_pen;
    CmykColor m_penCmyk;
    QRectF m_rect;       // 自定义包围矩形（由缩放手柄设置）
    QString m_filePath;
    QByteArray m_rawTiffData;  // 保留原始 TIFF 数据用于无损导出

    // CMYK 源像素数据
    QByteArray m_rawCmykPixels;  // 原始 CMYK 像素（4 bytes/pixel, libtiff 0-255 顺序）
    int m_cmykWidth = 0;
    int m_cmykHeight = 0;
    bool m_isCmykSource = false;
};

#endif // IMAGEITEM_H
