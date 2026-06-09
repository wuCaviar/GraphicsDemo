#include "CanvasItem.h"
#include <QPainter>

CanvasItem::CanvasItem(QGraphicsItem *parent)
    : QGraphicsRectItem(QRectF(0, 0, 2480.0, 3508.0), parent) // A4 默认 210x297mm @300ppi
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsFocusable, false);
    setBrush(QBrush(Qt::transparent)); // 透明，白色填充由 drawBackground 绘制
    setPen(QPen(Qt::NoPen));
    setZValue(-9999); // 始终在最底层
}

CanvasItem::CanvasItem(const QSizeF &sizePx, QGraphicsItem *parent)
    : QGraphicsRectItem(QRectF(0, 0, sizePx.width(), sizePx.height()), parent)
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsFocusable, false);
    setBrush(QBrush(Qt::transparent)); // 透明，白色填充由 drawBackground 绘制
    setPen(QPen(Qt::NoPen));
    setZValue(-9999);
}

CanvasItem::CanvasItem(qreal widthMm, qreal heightMm, qreal displayPpi,
                       QGraphicsItem *parent)
    : QGraphicsRectItem(QRectF(0, 0, widthMm * displayPpi / 25.4,
                               heightMm * displayPpi / 25.4), parent)
    , m_ppi(displayPpi)
    , m_physicalWidthMm(widthMm)
    , m_physicalHeightMm(heightMm)
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsFocusable, false);
    setBrush(QBrush(Qt::transparent));
    setPen(QPen(Qt::NoPen));
    setZValue(-9999);
}

void CanvasItem::setCanvasSize(const QSizeF &sizePx)
{
    setRect(QRectF(0, 0, sizePx.width(), sizePx.height()));
}

void CanvasItem::setCanvasSizeMm(qreal wMm, qreal hMm)
{
    m_physicalWidthMm = wMm;
    m_physicalHeightMm = hMm;
    updatePixelRect();
}

void CanvasItem::setPpi(qreal ppi)
{
    setDisplayPpi(ppi);
}

void CanvasItem::setDisplayPpi(qreal ppi)
{
    if (qFuzzyCompare(m_ppi, ppi))
        return;
    qreal oldPpi = m_ppi;
    m_ppi = qBound(1.0, ppi, 9999.0);
    updatePixelRect();
    // 像素尺寸变化比例 = m_ppi / oldPpi
    // 调用方需用此比例缩放所有图元
}

void CanvasItem::updatePixelRect()
{
    qreal pxW = m_physicalWidthMm * m_ppi / 25.4;
    qreal pxH = m_physicalHeightMm * m_ppi / 25.4;
    setRect(QRectF(0, 0, pxW, pxH));
}

qreal CanvasItem::pixelsPerMm() const
{
    return m_ppi / 25.4;
}

QSizeF CanvasItem::canvasSize() const
{
    return rect().size();
}

void CanvasItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(Qt::NoPen);
    painter->setBrush(brush());
    painter->drawRect(rect());

    // 绘制画布边框（浅灰阴影效果）
    painter->setPen(QPen(QColor(180, 180, 180), 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect());
}
