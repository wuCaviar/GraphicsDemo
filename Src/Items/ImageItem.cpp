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

static void writeCvMat(QDataStream &out, const cv::Mat &m)
{
    out << m.rows << m.cols << static_cast<int>(m.type());
    if (m.isContinuous() && !m.empty()) {
        QByteArray data(reinterpret_cast<const char *>(m.data),
                        static_cast<int>(m.total() * m.elemSize()));
        out << data;
    } else {
        out << QByteArray();
    }
}

static cv::Mat readCvMat(QDataStream &in)
{
    int rows, cols, type;
    QByteArray data;
    in >> rows >> cols >> type >> data;
    if (rows > 0 && cols > 0 && !data.isEmpty()) {
        cv::Mat m(rows, cols, type, data.data());
        return m.clone();
    }
    return cv::Mat();
}

void ImageItem::serialize(QDataStream &out) const
{
    QImage img = pixmap().toImage();
    out << img << m_pen << pos() << rotation() << m_filePath;
    writeCvMat(out, m_rawTiffMat);
    out << m_rect;
    out << m_isCmykSource;
    if (m_isCmykSource)
        writeCvMat(out, m_rawCmykMat);
}

bool ImageItem::deserialize(QDataStream &in)
{
    QImage img;
    qreal rot;
    QPointF pos_;
    in >> img >> m_pen >> pos_ >> rot >> m_filePath;
    m_rawTiffMat = readCvMat(in);
    in >> m_rect;
    if (in.status() != QDataStream::Ok)
        return false;

    m_isCmykSource = false;
    if (!in.atEnd()) {
        in >> m_isCmykSource;
        if (m_isCmykSource)
            m_rawCmykMat = readCvMat(in);
    }

    setPixmap(QPixmap::fromImage(img));
    setPos(pos_);
    setRotation(rot);
    return true;
}

void ImageItem::setCmykSourceData(const cv::Mat &data)
{
    m_rawCmykMat = data.clone();
    m_isCmykSource = true;
}
