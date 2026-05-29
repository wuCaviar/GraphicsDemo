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

    void setPpi(qreal ppi); // 设置 PPI，影响 mm 换算（0 表示无 DPI，默认 1mm=1scene unit）
    qreal ppi() const { return m_ppi; }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

public slots:
    void updateRuler();
    void setMousePosition(const QPointF &scenePos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal pixelsPerMm() const { return m_ppi > 0.0 ? (m_ppi / 25.4) : 1.0; }
    qreal toDisplayValue(qreal scenePixels) const;
    void calcInterval(qreal &interval, qreal &subInterval) const;
    QString formatLabel(qreal value) const;
    qreal sceneToScreen(qreal scenePos) const;

    RulerOrientation m_orientation;
    QGraphicsView *m_pView = nullptr;
    qreal m_scale = 1.0;
    qreal m_originPx = 0.0;
    qreal m_mousePos = -1.0;
    qreal m_ppi = 0.0; // 0 表示尚未由图片确定 DPI
};

#endif // RULERBAR_H
