#include "CanvasItem.h"
#include <QPainter>

CanvasItem::CanvasItem(QGraphicsItem *parent)
    : QGraphicsRectItem(QRectF(0, 0, 210, 297), parent) // A4 默认 210x297mm（无 DPI 时 1mm = 1 scene unit）
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsFocusable, false);
    setBrush(QBrush(Qt::transparent)); // 透明，白色填充由 drawBackground 绘制
    setPen(QPen(Qt::NoPen));
    setZValue(-9999); // 始终在最底层
}

CanvasItem::CanvasItem(const QSizeF &size, QGraphicsItem *parent)
    : QGraphicsRectItem(QRectF(0, 0, size.width(), size.height()), parent)
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsFocusable, false);
    setBrush(QBrush(Qt::transparent)); // 透明，白色填充由 drawBackground 绘制
    setPen(QPen(Qt::NoPen));
    setZValue(-9999);
}

void CanvasItem::setCanvasSize(const QSizeF &size)
{
    setRect(QRectF(0, 0, size.width(), size.height()));
}

void CanvasItem::setPpi(qreal ppi)
{
    m_ppi = (ppi <= 0.0) ? 0.0 : qBound(1.0, ppi, 9999.0);
}

void CanvasItem::setCanvasSizeMm(const QSizeF &sizeMm)
{
    qreal k = pixelsPerMm();
    setRect(QRectF(0, 0, sizeMm.width() * k, sizeMm.height() * k));
}

QSizeF CanvasItem::canvasSizeMm() const
{
    qreal k = pixelsPerMm();
    QSizeF sz = rect().size();
    return QSizeF(sz.width() / k, sz.height() / k);
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
