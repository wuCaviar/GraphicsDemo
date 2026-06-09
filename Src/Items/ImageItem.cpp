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

ImageItem::ImageItem(const QPixmap &pixmap, const QSize &sourcePixelSize,
                     QGraphicsItem *parent)
    : QGraphicsPixmapItem(pixmap, parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    m_rect = QRectF(QPointF(0, 0), sourcePixelSize);
}

QGraphicsItem *ImageItem::cloneItem() const
{
    auto *item = new ImageItem(pixmap());
    item->setItemPen(m_pen);
    if (m_penCmyk.valid)
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
    item->setOriginalPhysicalMm(m_originalPhysicalMm);
    item->setScale(m_scaleX, m_scaleY);
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

QPainterPath ImageItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

void ImageItem::setGeometryRect(const QRectF &r)
{
    prepareGeometryChange();
    qreal oldW = m_rect.width();
    qreal oldH = m_rect.height();
    m_rect = r;
    if (oldW > 1.0 && oldH > 1.0) {
        m_scaleX *= r.width() / oldW;
        m_scaleY *= r.height() / oldH;
    }
    update();
}

void ImageItem::setScale(qreal sx, qreal sy)
{
    m_scaleX = sx;
    m_scaleY = sy;
}

QSize ImageItem::targetOutputPixels(int exportDpi) const
{
    qreal physW = m_originalPhysicalMm.width() * m_scaleX;
    qreal physH = m_originalPhysicalMm.height() * m_scaleY;
    int pxW = qMax(1, qRound(physW / 25.4 * exportDpi));
    int pxH = qMax(1, qRound(physH / 25.4 * exportDpi));
    return QSize(pxW, pxH);
}

void ImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                      QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (pixmap().isNull() || !m_rect.isValid())
        return;
    painter->drawPixmap(m_rect, pixmap(), pixmap().rect());
}

void ImageItem::serialize(QDataStream &out) const
{
    out << pixmap() << pos() << rotation() << m_filePath;
    out << m_rect << m_originalSize << m_dpiX << m_dpiY << m_isCmykSource
        << m_isMultiPage;
    // 新格式标记: 2 表示后面有 scale + physicalMm 字段
    out << static_cast<quint8>(2);
    out << m_scaleX << m_scaleY << m_originalPhysicalMm.width()
        << m_originalPhysicalMm.height();
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

    m_scaleX = 1.0;
    m_scaleY = 1.0;

    QIODevice *dev = in.device();
    if (!dev->atEnd()) {
        quint8 marker;
        in >> marker;
        if (in.status() == QDataStream::Ok && marker == 2) {
            qreal physW = 0, physH = 0;
            in >> m_scaleX >> m_scaleY >> physW >> physH;
            if (in.status() == QDataStream::Ok && physW > 0 && physH > 0)
                m_originalPhysicalMm = QSizeF(physW, physH);
        }
        // 如果 marker == 1，说明是旧格式 CMYK 标记
        // readCmykIfAvailable 会通过 atEnd 回退处理 — 但已经消费了 marker
        // 所以需要手动处理
        if (in.status() == QDataStream::Ok && marker == 1) {
            double c, m, y, k;
            in >> c >> m >> y >> k;
            if (in.status() == QDataStream::Ok) {
                m_penCmyk = {c, m, y, k, true};
            }
        }
    }

    setPixmap(pix);
    setPos(pos_);
    setRotation(rot);

    // 回退计算物理尺寸（老项目兼容）
    if (!m_originalPhysicalMm.isValid() && m_dpiX > 0 && m_dpiY > 0
        && m_originalSize.isValid()) {
        m_originalPhysicalMm = QSizeF(
            m_originalSize.width() / static_cast<qreal>(m_dpiX) * 25.4,
            m_originalSize.height() / static_cast<qreal>(m_dpiY) * 25.4);
    }

    return true;
}
