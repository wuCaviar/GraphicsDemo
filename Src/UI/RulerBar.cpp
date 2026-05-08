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

    if (m_orientation == Horizontal) {
        m_offset = m_pView->horizontalScrollBar()->value() / m_scale;
    } else {
        m_offset = m_pView->verticalScrollBar()->value() / m_scale;
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

    // 根据缩放级别选择合适的刻度间隔
    qreal interval = 100;
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

    qreal subInterval = interval / 5.0; // 5 等分次刻度

    if (m_orientation == Horizontal) {
        // 水平刻度尺
        qreal range = width() / m_scale;
        qreal start = m_offset;

        qreal firstTick = qFloor(start / interval) * interval;
        qreal firstSubTick = qFloor(start / subInterval) * subInterval;

        // 绘制次刻度
        pen.setColor(minorTickColor);
        pen.setWidthF(0.5);
        painter.setPen(pen);
        for (qreal pos = firstSubTick; pos <= start + range + subInterval; pos += subInterval) {
            // 跳过主刻度位置
            if (qFuzzyIsNull(std::fmod(pos + interval * 0.5, interval)))
                continue;
            qreal screenX = (pos - start) * m_scale;
            if (screenX < -1 || screenX > width() + 1)
                continue;
            painter.drawLine(QPointF(screenX, kRulerSize), QPointF(screenX, kRulerSize - 4));
        }

        // 绘制主刻度和标签
        pen.setColor(tickColor);
        pen.setWidthF(1.0);
        painter.setPen(pen);
        for (qreal pos = firstTick; pos <= start + range + interval; pos += interval) {
            qreal screenX = (pos - start) * m_scale;
            if (screenX < -10 || screenX > width() + 10)
                continue;

            // 主刻度线
            painter.drawLine(QPointF(screenX, kRulerSize), QPointF(screenX, kRulerSize - 10));

            // 刻度标签
            painter.setPen(textColor);
            int val = qRound(pos);
            painter.drawText(QRectF(screenX - 25, 0, 50, kRulerSize - 11),
                             Qt::AlignCenter, QString::number(val));
            painter.setPen(pen);
        }

        // 底部分隔线
        QPen borderPen(QColor(200, 200, 200));
        borderPen.setWidthF(1.0);
        painter.setPen(borderPen);
        painter.drawLine(0, kRulerSize - 1, width(), kRulerSize - 1);

        // 鼠标位置指示器（红色三角 + 线）
        if (m_mousePos >= 0) {
            qreal screenX = (m_mousePos - start) * m_scale;
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
        qreal range = height() / m_scale;
        qreal start = m_offset;

        qreal firstTick = qFloor(start / interval) * interval;
        qreal firstSubTick = qFloor(start / subInterval) * subInterval;

        // 绘制次刻度
        pen.setColor(minorTickColor);
        pen.setWidthF(0.5);
        painter.setPen(pen);
        for (qreal pos = firstSubTick; pos <= start + range + subInterval; pos += subInterval) {
            if (qFuzzyIsNull(std::fmod(pos + interval * 0.5, interval)))
                continue;
            qreal screenY = (pos - start) * m_scale;
            if (screenY < -1 || screenY > height() + 1)
                continue;
            painter.drawLine(QPointF(kRulerSize, screenY), QPointF(kRulerSize - 4, screenY));
        }

        // 绘制主刻度和标签
        pen.setColor(tickColor);
        pen.setWidthF(1.0);
        painter.setPen(pen);
        for (qreal pos = firstTick; pos <= start + range + interval; pos += interval) {
            qreal screenY = (pos - start) * m_scale;
            if (screenY < -10 || screenY > height() + 10)
                continue;

            // 主刻度线
            painter.drawLine(QPointF(kRulerSize, screenY), QPointF(kRulerSize - 10, screenY));

            // 绘制旋转文字标签
            painter.save();
            painter.setPen(textColor);
            int val = qRound(pos);
            painter.translate(kRulerSize - 11, screenY);
            painter.rotate(-90);
            painter.drawText(QRectF(-25, -2, 50, 14), Qt::AlignCenter, QString::number(val));
            painter.restore();
            painter.setPen(pen);
        }

        // 右侧分隔线
        QPen borderPen(QColor(200, 200, 200));
        borderPen.setWidthF(1.0);
        painter.setPen(borderPen);
        painter.drawLine(kRulerSize - 1, 0, kRulerSize - 1, height());

        // 鼠标位置指示器
        if (m_mousePos >= 0) {
            qreal screenY = (m_mousePos - start) * m_scale;
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
