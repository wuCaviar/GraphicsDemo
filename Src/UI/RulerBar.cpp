#include "RulerBar.h"

#include <QGraphicsView>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QEvent>
#include <QtMath>
#include <cmath>

static const int kRulerSize = 30; // 刻度尺宽度/高度（加宽以提升可读性）

// 将数值规整到 1-2-5 序列中最接近的"整数"（保持屏幕刻度间距可读）
static qreal roundToNiceNumber(qreal value)
{
    if (value <= 0.0)
        return 1.0;
    const qreal magnitude = std::pow(10.0, std::floor(std::log10(value)));
    const qreal normalized = value / magnitude; // [1, 10)
    qreal nice;
    if (normalized < 1.5)
        nice = 1.0;
    else if (normalized < 3.5)
        nice = 2.0;
    else if (normalized < 7.5)
        nice = 5.0;
    else
        nice = 10.0;
    return nice * magnitude;
}

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

void RulerBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange) {
        update();
    }
    QWidget::changeEvent(event);
}

void RulerBar::updateRuler()
{
    if (!m_pView)
        return;

    QTransform transform = m_pView->transform();
    m_scale = transform.m11();

    // 计算画布原点 (0,0) 在刻度尺上的屏幕像素位置
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
    return scenePixels / (m_ppi / 25.4);
}

qreal RulerBar::sceneToScreen(qreal scenePos) const
{
    return m_originPx + scenePos * m_scale;
}

void RulerBar::calcInterval(qreal &interval, qreal &subInterval) const
{
    static constexpr qreal kTargetSpacing = 80.0;

    const qreal pixelsPerMm = m_ppi / 25.4;
    const qreal mmScreenPx = pixelsPerMm * m_scale;
    const qreal idealMm = kTargetSpacing / mmScreenPx;
    const qreal intervalMm = roundToNiceNumber(idealMm);

    qreal subMm;
    qreal intPart;
    qreal frac = std::modf(intervalMm, &intPart);
    if (qFuzzyCompare(frac, 0.0)) {
        int n = qRound(intPart);
        if (n % 5 == 0)
            subMm = intervalMm / 5.0;
        else if (n % 2 == 0)
            subMm = intervalMm / 4.0;
        else
            subMm = intervalMm / 5.0;
    } else {
        subMm = intervalMm / 5.0;
    }

    interval = intervalMm * pixelsPerMm;
    subInterval = subMm * pixelsPerMm;
}

QString RulerBar::formatLabel(qreal value) const
{
    if (qFuzzyCompare(value, qRound(value)))
        return QString::number(qRound(value));
    if (qAbs(value) >= 1.0)
        return QString::number(value, 'f', 1);
    return QString::number(value, 'f', 2);
}

void RulerBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Derive all colors from the widget's palette (set by QSS).
    // QSS QWidget rule sets Window + WindowText; we derive the rest
    // programmatically so rulers always match the active stylesheet.
    const QPalette &pal = palette();
    QColor bg = pal.color(QPalette::Window);
    QColor fg = pal.color(QPalette::WindowText);

    // bgAlt = bg shifted ~5% toward black
    QColor bgAlt = bg.darker(108);
    // mid = halfway between bg and fg
    QColor mid = QColor::fromRgb((bg.red() + fg.red()) / 2, (bg.green() + fg.green()) / 2,
                                 (bg.blue() + fg.blue()) / 2);
    // dark = fg at ~60% opacity over bg (visible but subdued)
    QColor dark = QColor::fromRgb((bg.red() * 2 + fg.red()) / 3, (bg.green() * 2 + fg.green()) / 3,
                                  (bg.blue() * 2 + fg.blue()) / 3);

    QColor borderColor = mid.lighter(115);
    QColor unitColor = mid;

    // 渐变背景
    QLinearGradient gradient;
    if (m_orientation == Horizontal) {
        gradient = QLinearGradient(0, 0, 0, kRulerSize);
        gradient.setColorAt(0, bg);
        gradient.setColorAt(1, bgAlt);
    } else {
        gradient = QLinearGradient(0, 0, kRulerSize, 0);
        gradient.setColorAt(0, bg);
        gradient.setColorAt(1, bgAlt);
    }
    painter.fillRect(rect(), gradient);

    QPen pen(dark);
    pen.setWidthF(1.0);
    painter.setPen(pen);

    QFont font("SF Pro Display", 8);
    font.setStyleStrategy(QFont::PreferAntialias);
    painter.setFont(font);

    // 计算刻度间隔
    qreal interval, subInterval;
    calcInterval(interval, subInterval);

    if (m_orientation == Horizontal) {
        qreal visibleLeft = -m_originPx / m_scale;
        qreal visibleRight = (width() - m_originPx) / m_scale;

        qreal firstTick = qFloor(visibleLeft / interval) * interval;
        qreal firstSubTick = qFloor(visibleLeft / subInterval) * subInterval;

        // 绘制次刻度
        pen.setColor(mid);
        pen.setWidthF(0.5);
        painter.setPen(pen);
        for (qreal pos = firstSubTick; pos <= visibleRight + subInterval; pos += subInterval) {
            if (qFuzzyIsNull(std::fmod(pos + interval * 0.5, interval)))
                continue;
            qreal screenX = sceneToScreen(pos);
            if (screenX < -1 || screenX > width() + 1)
                continue;
            painter.drawLine(QPointF(screenX, kRulerSize), QPointF(screenX, kRulerSize - 4));
        }

        // 绘制主刻度和标签
        pen.setColor(dark);
        pen.setWidthF(1.0);
        painter.setPen(pen);
        for (qreal pos = firstTick; pos <= visibleRight + interval; pos += interval) {
            qreal screenX = sceneToScreen(pos);
            if (screenX < -10 || screenX > width() + 10)
                continue;

            painter.drawLine(QPointF(screenX, kRulerSize), QPointF(screenX, kRulerSize - 10));

            painter.setPen(fg);
            qreal displayVal = toDisplayValue(pos);
            painter.drawText(QRectF(screenX - 25, 0, 50, kRulerSize - 11), Qt::AlignCenter,
                             formatLabel(displayVal));
            painter.setPen(pen);
        }

        // 底部分隔线
        QPen borderPen(borderColor);
        borderPen.setWidthF(1.0);
        painter.setPen(borderPen);
        painter.drawLine(0, kRulerSize - 1, width(), kRulerSize - 1);

        // 单位标识（右下角）
        painter.setPen(unitColor);
        QFont unitFont("SF Pro Display", 7);
        painter.setFont(unitFont);
        QString unitLabel = QStringLiteral("mm");
        painter.drawText(QRectF(width() - 24, kRulerSize - 14, 22, 12),
                         Qt::AlignRight | Qt::AlignBottom, unitLabel);

        // 鼠标位置指示器
        if (m_mousePos >= 0) {
            qreal screenX = sceneToScreen(m_mousePos);
            if (screenX >= 0 && screenX <= width()) {
                QPen indicatorPen(QColor(220, 50, 50), 1.0);
                painter.setPen(indicatorPen);
                painter.drawLine(QPointF(screenX, 0), QPointF(screenX, kRulerSize - 1));

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
        qreal visibleTop = -m_originPx / m_scale;
        qreal visibleBottom = (height() - m_originPx) / m_scale;

        qreal firstTick = qFloor(visibleTop / interval) * interval;
        qreal firstSubTick = qFloor(visibleTop / subInterval) * subInterval;

        // 绘制次刻度
        pen.setColor(mid);
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
        pen.setColor(dark);
        pen.setWidthF(1.0);
        painter.setPen(pen);
        for (qreal pos = firstTick; pos <= visibleBottom + interval; pos += interval) {
            qreal screenY = sceneToScreen(pos);
            if (screenY < -10 || screenY > height() + 10)
                continue;

            painter.drawLine(QPointF(kRulerSize, screenY), QPointF(kRulerSize - 10, screenY));

            painter.save();
            painter.setPen(fg);
            qreal displayVal = toDisplayValue(pos);
            painter.translate(kRulerSize - 11, screenY);
            painter.rotate(-90);
            painter.drawText(QRectF(-25, -2, 50, 14), Qt::AlignCenter, formatLabel(displayVal));
            painter.restore();
            painter.setPen(pen);
        }

        // 右侧分隔线
        QPen borderPen(borderColor);
        borderPen.setWidthF(1.0);
        painter.setPen(borderPen);
        painter.drawLine(kRulerSize - 1, 0, kRulerSize - 1, height());

        // 单位标识（右下角）
        painter.setPen(unitColor);
        QFont unitFont("SF Pro Display", 7);
        painter.setFont(unitFont);
        QString unitLabel = QStringLiteral("mm");
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
