#ifndef RULERBAR_H
#define RULERBAR_H

#include <QWidget>

class QGraphicsView;

// 刻度尺控件 — 可显示在视图左侧和顶部
class RulerBar : public QWidget
{
    Q_OBJECT

public:
    enum RulerOrientation { Horizontal, Vertical };

    explicit RulerBar(RulerOrientation orientation, QWidget *parent = nullptr);

    void setGraphicsView(QGraphicsView *view);
    void setRulerUnit(qreal pixelsPerUnit); // 每单位像素数（用于刻度间隔）

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

public slots:
    void updateRuler();
    void setMousePosition(const QPointF &scenePos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    RulerOrientation m_orientation;
    QGraphicsView *m_pView = nullptr;
    qreal m_pixelsPerUnit = 1.0; // 默认 1px/unit
    qreal m_offset = 0.0;        // 滚动偏移
    qreal m_scale = 1.0;         // 缩放系数
    qreal m_mousePos = -1.0;     // 鼠标在刻度尺方向上的场景坐标（-1 表示无效）
};

#endif // RULERBAR_H
