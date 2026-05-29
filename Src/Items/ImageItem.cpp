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
    item->setItemPenCmyk(m_penCmyk.c, m_penCmyk.m, m_penCmyk.y, m_penCmyk.k);
    item->setPos(pos());
    item->setRotation(rotation());
    item->setTransform(transform());
    item->setTransformOriginPoint(transformOriginPoint());
    if (m_rect.isValid())
        item->setRect(m_rect);
    item->setFilePath(m_filePath);
    item->setCmykSource(m_isCmykSource);
    item->setMultiPageSource(m_isMultiPage);
    item->setOriginalSize(m_originalSize);
    item->setDpi(m_dpiX, m_dpiY);
    return item;
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

void ImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                      QWidget *widget)
{
    QGraphicsPixmapItem::paint(painter, option, widget);
}

void ImageItem::serialize(QDataStream &out) const
{
    out << pixmap() << pos() << rotation() << m_filePath;
    out << m_rect << m_originalSize << m_dpiX << m_dpiY << m_isCmykSource
        << m_isMultiPage;
    writeCmykIfValid(out, m_penCmyk);
}

bool ImageItem::deserialize(QDataStream &in)
{
    QPixmap pix;
    qreal rot;
    QPointF pos_;
    in >> pix >> pos_ >> rot >> m_filePath;
    in >> m_rect >> m_originalSize >> m_dpiX >> m_dpiY >> m_isCmykSource
        >> m_isMultiPage;
    if (in.status() != QDataStream::Ok)
        return false;

    setPixmap(pix);
    setPos(pos_);
    setRotation(rot);
    if (!readCmykIfAvailable(in, in.device(), m_penCmyk))
        return false;
    return true;
}
