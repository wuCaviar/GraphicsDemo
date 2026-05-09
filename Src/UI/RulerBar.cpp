#include "RulerBar.h"

#include <QGraphicsView>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QtMath>
#include <cmath>

static const int kRulerSize = 30; // 刻度尺宽度/高度（加宽以提升可读性）

RulerBar::RulerBar(RulerOrientation orientation, QWidget *parent)
    : QWidget(parent), m_orientation(orientation)
{
    if (orientation == Horizontal) {
        setFixedHeight(kRulerSize);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else {
        setFixedWidth(kRulerSize);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    setMouseTracking(true);
}

void RulerBar::setGraphicsView(QGraphicsView *view)
{
    m_pView = view;
    updateRuler();
}

void RulerBar::setRulerUnit(qreal pixelsPerUnit)
{
    m_pixelsPerUnit = qMax(0.01, pixelsPerUnit);
    update();
}

void RulerBar::setUnit(RulerUnit unit)
{
    if (m_unit == unit)
        return;
    m_unit = unit;
    update();
}

void RulerBar::setPpi(qreal ppi)
{
    m_ppi = qBound(1.0, ppi, 9999.0);
    update();
}

QSize RulerBar::minimumSizeHint() const
{
    return m_orientation == Horizontal ? QSize(50, kRulerSize) : QSize(kRulerSize, 50);
}

QSize RulerBar::sizeHint() const
{
    return minimumSizeHint();
}

void RulerBar::updateRuler()
{
    if (!m_pView)
        return;

    QTransform transform = m_pView->transform();
    m_scale = transform.m11();

    // 计算画布原点 (0,0) 在刻度尺上的屏幕像素位置
    // mapFromScene → viewport 坐标，再 mapToParent → QGraphicsView widget 坐标
    // 刻度尺 widget 与 QGraphicsView widget 在同一 grid layout 中对齐
    QPoint vpOrigin = m_pView->mapFromScene(QPointF(0, 0));
    QPoint widgetOrigin = m_pView->viewport()->mapToParent(vpOrigin);
    if (m_orientation == Horizontal) {
        m_originPx = widgetOrigin.x();
    } else {
        m_originPx = widgetOrigin.y();
    }

    update();
}

void RulerBar::setMousePosition(const QPointF &scenePos)
{
    qreal newPos = (m_orientation == Horizontal) ? scenePos.x() : scenePos.y();
    if (!qFuzzyCompare(m_mousePos, newPos)) {
        m_mousePos = newPos;
        update();
    }
}

qreal RulerBar::toDisplayValue(qreal scenePixels) const
{
    if (m_unit == Millimeter)
        return scenePixels / (m_ppi / 25.4);
    return scenePixels;
}

qreal RulerBar::sceneToScreen(qreal scenePos) const
{
    // 场景坐标 scenePos → 刻度尺上的屏幕像素位置
    // 画布原点 (0,0) 在屏幕位置 m_originPx
    // 偏移 = scenePos * m_scale（缩放后的像素偏移）
    return m_originPx + scenePos * m_scale;
}

void RulerBar::calcInterval(qreal &interval, qreal &subInterval) const
{
    if (m_unit == Pixel) {
        // px 模式：原有逻辑
        if (m_scale < 0.15)
            interval = 1000;
        else if (m_scale < 0.3)
            interval = 500;
        else if (m_scale < 0.6)
            interval = 200;
        else if (m_scale < 1.2)
            interval = 100;
        else if (m_scale < 2.5)
            interval = 50;
        else if (m_scale < 5.0)
            interval = 20;
        else
            interval = 10;
        subInterval = interval / 5.0;
    } else {
        // mm 模式：刻度间隔以 mm 为单位，选择 1-2-5 序列
        // 先计算 1mm 对应的屏幕像素数
        qreal pixelsPerMm = m_ppi / 25.4;
        qreal mmScreenPx = pixelsPerMm * m_scale;

        // 选择合适的 mm 间隔，使得主刻度屏幕间距在 40~150px 之间
        // 候选序列：0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500
        static const qreal candidates[] = {
            0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500
        };

        interval = 10; // 默认 10mm
        for (qreal c : candidates) {
            qreal screenSpacing = c * mmScreenPx;
            if (screenSpacing >= 40 && screenSpacing <= 150) {
                interval = c;
                break;
            }
        }

        // 次刻度：将主刻度 5 等分或 2 等分
        // 对于 1, 10, 100 等 → 5 等分（0.2, 2, 20）
        // 对于 0.2, 2, 20, 200 等 → 4 等分（0.05, 0.5, 5, 50）
        // 对于 0.5, 5, 50, 500 等 → 5 等分（0.1, 1, 10, 100）
        qreal intPart;
        qreal frac = std::modf(interval, &intPart);
        if (qFuzzyCompare(frac, 0.0)) {
            // 整数间隔：1, 2, 5, 10, 20, 50, 100...
            int n = qRound(intPart);
            if (n % 5 == 0)
                subInterval = interval / 5.0;   // 5→1, 10→2, 50→10, 100→20
            else if (n % 2 == 0)
                subInterval = interval / 4.0;   // 2→0.5, 20→5, 200→50
            else
                subInterval = interval / 5.0;   // 1→0.2
        } else {
            subInterval = interval / 5.0;       // 0.1→0.02, 0.2→0.04, 0.5→0.1
        }

        // 将 mm 单位的间隔转换为场景像素单位（后续绘制代码用场景像素计算）
        interval *= pixelsPerMm;
        subInterval *= pixelsPerMm;
    }
}

QString RulerBar::formatLabel(qreal value) const
{
    if (m_unit == Millimeter) {
        // mm 模式：智能格式化
        if (qFuzzyCompare(value, qRound(value)))
            return QString::number(qRound(value));
        if (qAbs(value) >= 1.0)
            return QString::number(value, 'f', 1);
        return QString::number(value, 'f', 2);
    }
    // px 模式：整数
    return QString::number(qRound(value));
}

void RulerBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // 渐变背景
    QLinearGradient gradient;
    if (m_orientation == Horizontal) {
        gradient = QLinearGradient(0, 0, 0, kRulerSize);
        gradient.setColorAt(0, QColor(248, 248, 248));
        gradient.setColorAt(1, QColor(235, 235, 235));
    } else {
        gradient = QLinearGradient(0, 0, kRulerSize, 0);
        gradient.setColorAt(0, QColor(248, 248, 248));
        gradient.setColorAt(1, QColor(235, 235, 235));
    }
    painter.fillRect(rect(), gradient);

    // 刻度线颜色
    QColor tickColor(120, 120, 120);
    QColor minorTickColor(170, 170, 170);
    QColor textColor(80, 80, 80);

    QPen pen(tickColor);
    pen.setWidthF(1.0);
    painter.setPen(pen);

    QFont font("SF Pro Display", 8);
    font.setStyleStrategy(QFont::PreferAntialias);
    painter.setFont(font);

    // 计算刻度间隔
    qreal interval, subInterval;
    calcInterval(interval, subInterval);

    if (m_orientation == Horizontal) {
        // 水平刻度尺
        // 计算可见区域对应的场景坐标范围
        qreal visibleLeft  = -m_originPx / m_scale;                   // 刻度尺左边缘对应的场景坐标
        qreal visibleRight = (width() - m_originPx) / m_scale;        // 刻度尺右边缘对应的场景坐标

        qreal firstTick    = qFloor(visibleLeft / interval) * interval;
        qreal firstSubTick = qFloor(visibleLeft / subInterval) * subInterval;

        // 绘制次刻度
        pen.setColor(minorTickColor);
        pen.setWidthF(0.5);
        painter.setPen(pen);
        for (qreal pos = firstSubTick; pos <= visibleRight + subInterval; pos += subInterval) {
            // 跳过主刻度位置
            if (qFuzzyIsNull(std::fmod(pos + interval * 0.5, interval)))
                continue;
            qreal screenX = sceneToScreen(pos);
            if (screenX < -1 || screenX > width() + 1)
                continue;
            painter.drawLine(QPointF(screenX, kRulerSize), QPointF(screenX, kRulerSize - 4));
        }

        // 绘制主刻度和标签
        pen.setColor(tickColor);
        pen.setWidthF(1.0);
        painter.setPen(pen);
        for (qreal pos = firstTick; pos <= visibleRight + interval; pos += interval) {
            qreal screenX = sceneToScreen(pos);
            if (screenX < -10 || screenX > width() + 10)
                continue;

            // 主刻度线
            painter.drawLine(QPointF(screenX, kRulerSize), QPointF(screenX, kRulerSize - 10));

            // 刻度标签
            painter.setPen(textColor);
            qreal displayVal = toDisplayValue(pos);
            painter.drawText(QRectF(screenX - 25, 0, 50, kRulerSize - 11),
                             Qt::AlignCenter, formatLabel(displayVal));
            painter.setPen(pen);
        }

        // 底部分隔线
        QPen borderPen(QColor(200, 200, 200));
        borderPen.setWidthF(1.0);
        painter.setPen(borderPen);
        painter.drawLine(0, kRulerSize - 1, width(), kRulerSize - 1);

        // 单位标识（右下角）
        painter.setPen(QColor(140, 140, 140));
        QFont unitFont("SF Pro Display", 7);
        painter.setFont(unitFont);
        QString unitLabel = (m_unit == Millimeter) ? QStringLiteral("mm") : QStringLiteral("px");
        painter.drawText(QRectF(width() - 24, kRulerSize - 14, 22, 12),
                         Qt::AlignRight | Qt::AlignBottom, unitLabel);

        // 鼠标位置指示器（红色三角 + 线）
        if (m_mousePos >= 0) {
            qreal screenX = sceneToScreen(m_mousePos);
            if (screenX >= 0 && screenX <= width()) {
                // 红色指示线
                QPen indicatorPen(QColor(220, 50, 50), 1.0);
                painter.setPen(indicatorPen);
                painter.drawLine(QPointF(screenX, 0), QPointF(screenX, kRulerSize - 1));

                // 红色三角标记
                QPainterPath tri;
                tri.moveTo(screenX - 4, 0);
                tri.lineTo(screenX + 4, 0);
                tri.lineTo(screenX, 5);
                tri.closeSubpath();
                painter.fillPath(tri, QColor(220, 50, 50));
            }
        }
    } else {
        // 垂直刻度尺
        qreal visibleTop    = -m_originPx / m_scale;
        qreal visibleBottom = (height() - m_originPx) / m_scale;

        qreal firstTick    = qFloor(visibleTop / interval) * interval;
        qreal firstSubTick = qFloor(visibleTop / subInterval) * subInterval;

        // 绘制次刻度
        pen.setColor(minorTickColor);
        pen.setWidthF(0.5);
        painter.setPen(pen);
        for (qreal pos = firstSubTick; pos <= visibleBottom + subInterval; pos += subInterval) {
            if (qFuzzyIsNull(std::fmod(pos + interval * 0.5, interval)))
                continue;
            qreal screenY = sceneToScreen(pos);
            if (screenY < -1 || screenY > height() + 1)
                continue;
            painter.drawLine(QPointF(kRulerSize, screenY), QPointF(kRulerSize - 4, screenY));
        }

        // 绘制主刻度和标签
        pen.setColor(tickColor);
        pen.setWidthF(1.0);
        painter.setPen(pen);
        for (qreal pos = firstTick; pos <= visibleBottom + interval; pos += interval) {
            qreal screenY = sceneToScreen(pos);
            if (screenY < -10 || screenY > height() + 10)
                continue;

            // 主刻度线
            painter.drawLine(QPointF(kRulerSize, screenY), QPointF(kRulerSize - 10, screenY));

            // 绘制旋转文字标签
            painter.save();
            painter.setPen(textColor);
            qreal displayVal = toDisplayValue(pos);
            painter.translate(kRulerSize - 11, screenY);
            painter.rotate(-90);
            painter.drawText(QRectF(-25, -2, 50, 14), Qt::AlignCenter, formatLabel(displayVal));
            painter.restore();
            painter.setPen(pen);
        }

        // 右侧分隔线
        QPen borderPen(QColor(200, 200, 200));
        borderPen.setWidthF(1.0);
        painter.setPen(borderPen);
        painter.drawLine(kRulerSize - 1, 0, kRulerSize - 1, height());

        // 单位标识（右下角）
        painter.setPen(QColor(140, 140, 140));
        QFont unitFont("SF Pro Display", 7);
        painter.setFont(unitFont);
        QString unitLabel = (m_unit == Millimeter) ? QStringLiteral("mm") : QStringLiteral("px");
        painter.save();
        painter.translate(10, height() - 4);
        painter.rotate(-90);
        painter.drawText(QRectF(-20, -8, 40, 12), Qt::AlignCenter, unitLabel);
        painter.restore();

        // 鼠标位置指示器
        if (m_mousePos >= 0) {
            qreal screenY = sceneToScreen(m_mousePos);
            if (screenY >= 0 && screenY <= height()) {
                QPen indicatorPen(QColor(220, 50, 50), 1.0);
                painter.setPen(indicatorPen);
                painter.drawLine(QPointF(0, screenY), QPointF(kRulerSize - 1, screenY));

                // 红色三角标记
                QPainterPath tri;
                tri.moveTo(0, screenY - 4);
                tri.lineTo(0, screenY + 4);
                tri.lineTo(5, screenY);
                tri.closeSubpath();
                painter.fillPath(tri, QColor(220, 50, 50));
            }
        }
    }
}
