#include "ImageItem.h"
#include <QPainter>

ImageItem::ImageItem(QGraphicsItem *parent) : QGraphicsPixmapItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
}

ImageItem::ImageItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsPixmapItem(pixmap, parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    // 初始化 m_rect 为原始图片尺寸
    if (!pixmap.isNull())
        m_rect = QRectF(QPointF(0, 0), pixmap.size());
}

QGraphicsItem *ImageItem::cloneItem() const
{
    auto *item = new ImageItem(pixmap());
    item->setItemPen(m_pen);
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    item->setTransformOriginPoint(transformOriginPoint());
    if (m_rect.isValid())
        item->setRect(m_rect);
    item->setFilePath(m_filePath);
    item->setRawTiffData(m_rawTiffData);
    if (m_isCmykSource)
        item->setCmykSourceData(m_rawCmykPixels, m_cmykWidth, m_cmykHeight);
    return item;
}

QImage ImageItem::itemImage() const { return pixmap().toImage(); }

void ImageItem::setItemImage(const QImage &img)
{
    prepareGeometryChange();
    setPixmap(QPixmap::fromImage(img));
    // 更新 m_rect
    if (!img.isNull())
        m_rect = QRectF(QPointF(0, 0), img.size());
    else
        m_rect = QRectF();
    update();
}

QRectF ImageItem::rect() const
{
    return m_rect.isValid() ? m_rect : QGraphicsPixmapItem::boundingRect();
}

void ImageItem::setRect(const QRectF &rect)
{
    prepareGeometryChange();
    m_rect = rect;
    update();
}

QRectF ImageItem::boundingRect() const
{
    return m_rect.isValid() ? m_rect : QGraphicsPixmapItem::boundingRect();
}

QRectF ImageItem::geometryRect() const
{
    // ImageItem 没有画笔边距问题，geometryRect 与 boundingRect 一致
    return boundingRect();
}

void ImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (m_rect.isValid() && !pixmap().isNull()) {
        // 按 m_rect 缩放绘制图片
        painter->drawPixmap(m_rect, pixmap(), QRectF(pixmap().rect()));
    } else {
        QGraphicsPixmapItem::paint(painter, option, widget);
    }
}

void ImageItem::serialize(QDataStream &out) const
{
    QImage img = pixmap().toImage();
    out << img << m_pen << pos() << rotation() << m_filePath << m_rawTiffData << m_rect;
    out << m_isCmykSource;
    if (m_isCmykSource)
        out << m_rawCmykPixels << m_cmykWidth << m_cmykHeight;
}

bool ImageItem::deserialize(QDataStream &in)
{
    QImage img;
    qreal rot;
    QPointF pos_;
    in >> img >> m_pen >> pos_ >> rot >> m_filePath >> m_rawTiffData >> m_rect;
    if (in.status() != QDataStream::Ok)
        return false;

    // 读取 CMYK 源数据（版本兼容：如果没有则跳过）
    m_isCmykSource = false;
    if (!in.atEnd()) {
        in >> m_isCmykSource;
        if (m_isCmykSource)
            in >> m_rawCmykPixels >> m_cmykWidth >> m_cmykHeight;
    }

    setPixmap(QPixmap::fromImage(img));
    setPos(pos_);
    setRotation(rot);
    return true;
}

void ImageItem::setCmykSourceData(const QByteArray &data, int w, int h)
{
    m_rawCmykPixels = data;
    m_cmykWidth = w;
    m_cmykHeight = h;
    m_isCmykSource = true;
}
