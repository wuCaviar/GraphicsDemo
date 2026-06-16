#include "CanvasItem.h"
#include <QPainter>

CanvasItem::CanvasItem(QGraphicsItem *parent)
    : QGraphicsRectItem(QRectF(0, 0, 1240.0, 1754.0), parent) // A4 默认 210x297mm @150ppi
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
    m_ppi = AtMath::clamp(ppi, 1.0, 9999.0);
}

void CanvasItem::setCanvasDpi(int dpiX, int dpiY)
{
    m_canvasDpiX = dpiX;
    m_canvasDpiY = dpiY;
    updateEffectivePpi();
}

void CanvasItem::lockDpi()
{
    m_dpiLocked = true;
}

void CanvasItem::unlockDpi()
{
    m_dpiLocked = false;
    m_canvasDpiX = 0;
    m_canvasDpiY = 0;
}

void CanvasItem::updateEffectivePpi()
{
    if (m_canvasDpiX > 0) {
        m_ppi = static_cast<qreal>(m_canvasDpiX);
    }
    // m_canvasDpiX = 0 时保持 m_ppi 不变（使用默认 300 或已设置的值）
}

qreal CanvasItem::pixelsPerMm() const
{
    // 始终基于有效 PPI 返回换算因子
    return AtMath::Units::DPIContext(m_ppi).mmToPx(1.0);
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
