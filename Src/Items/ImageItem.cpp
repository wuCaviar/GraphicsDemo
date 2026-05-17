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
    item->setRawTiffData(m_rawTiffMat);
    if (m_isCmykSource)
        item->setCmykSourceData(m_rawCmykMat);
    return item;
}

QImage ImageItem::itemImage() const { return pixmap().toImage(); }

void ImageItem::setItemImage(const QImage &img)
{
    prepareGeometryChange();
    setPixmap(QPixmap::fromImage(img));
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
    return boundingRect();
}

void ImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (m_rect.isValid() && !pixmap().isNull()) {
        painter->drawPixmap(m_rect, pixmap(), QRectF(pixmap().rect()));
    } else {
        QGraphicsPixmapItem::paint(painter, option, widget);
    }
}

static void writePixelBuffer(QDataStream &out, const ImageUtils::RawPixelBuffer &buf)
{
    out << buf.width << buf.height << buf.data;
}

static ImageUtils::RawPixelBuffer readPixelBuffer(QDataStream &in)
{
    ImageUtils::RawPixelBuffer buf;
    in >> buf.width >> buf.height >> buf.data;
    if (buf.width <= 0 || buf.height <= 0 || buf.data.isEmpty()) {
        buf.width = 0;
        buf.height = 0;
        buf.data.clear();
    }
    return buf;
}

void ImageItem::serialize(QDataStream &out) const
{
    QImage img = pixmap().toImage();
    out << img << m_pen << pos() << rotation() << m_filePath;
    writePixelBuffer(out, m_rawTiffMat);
    out << m_rect;
    out << m_isCmykSource;
    if (m_isCmykSource)
        writePixelBuffer(out, m_rawCmykMat);
}

bool ImageItem::deserialize(QDataStream &in)
{
    QImage img;
    qreal rot;
    QPointF pos_;
    in >> img >> m_pen >> pos_ >> rot >> m_filePath;
    m_rawTiffMat = readPixelBuffer(in);
    in >> m_rect;
    if (in.status() != QDataStream::Ok)
        return false;

    m_isCmykSource = false;
    if (!in.atEnd()) {
        in >> m_isCmykSource;
        if (m_isCmykSource)
            m_rawCmykMat = readPixelBuffer(in);
    }

    setPixmap(QPixmap::fromImage(img));
    setPos(pos_);
    setRotation(rot);
    return true;
}

void ImageItem::setCmykSourceData(const ImageUtils::RawPixelBuffer &data)
{
    m_rawCmykMat = data;
    m_isCmykSource = true;
}
