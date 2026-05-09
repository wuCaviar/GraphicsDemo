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
    enum RulerUnit { Pixel, Millimeter };

    explicit RulerBar(RulerOrientation orientation, QWidget *parent = nullptr);

    void setGraphicsView(QGraphicsView *view);
    void setRulerUnit(qreal pixelsPerUnit); // 每单位像素数（用于刻度间隔）

    RulerUnit unit() const { return m_unit; }
    void setUnit(RulerUnit unit);

    void setPpi(qreal ppi); // 设置 PPI，影响 mm 换算
    qreal ppi() const { return m_ppi; }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

public slots:
    void updateRuler();
    void setMousePosition(const QPointF &scenePos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // 根据单位将场景像素坐标转换为显示值
    qreal toDisplayValue(qreal scenePixels) const;
    // 根据单位获取刻度间隔（返回场景像素单位）
    void calcInterval(qreal &interval, qreal &subInterval) const;
    // 格式化刻度标签
    QString formatLabel(qreal value) const;
    // 将场景坐标转换为刻度尺控件上的屏幕像素位置
    qreal sceneToScreen(qreal scenePos) const;

    RulerOrientation m_orientation;
    RulerUnit m_unit = Pixel;
    QGraphicsView *m_pView = nullptr;
    qreal m_pixelsPerUnit = 1.0; // 默认 1px/unit
    qreal m_scale = 1.0;         // 缩放系数
    qreal m_originPx = 0.0;      // 画布原点 (0,0) 在刻度尺上的屏幕像素位置
    qreal m_mousePos = -1.0;     // 鼠标在刻度尺方向上的场景坐标（-1 表示无效）
    qreal m_ppi = 96.0;          // 当前 PPI（影响 mm 换算）
};

#endif // RULERBAR_H
