#include "CanvasItem.h"
#include <QPainter>

CanvasItem::CanvasItem(QGraphicsItem *parent)
    : QGraphicsRectItem(QRectF(0, 0, 793.7, 1122.5), parent) // A4 默认 210x297mm @96dpi
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsFocusable, false);
    setBrush(QBrush(Qt::white));
    setPen(QPen(Qt::NoPen));
    setZValue(-9999); // 始终在最底层
}

CanvasItem::CanvasItem(const QSizeF &size, QGraphicsItem *parent)
    : QGraphicsRectItem(QRectF(0, 0, size.width(), size.height()), parent)
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsFocusable, false);
    setBrush(QBrush(Qt::white));
    setPen(QPen(Qt::NoPen));
    setZValue(-9999);
}

void CanvasItem::setCanvasSize(const QSizeF &size)
{
    setRect(QRectF(0, 0, size.width(), size.height()));
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
