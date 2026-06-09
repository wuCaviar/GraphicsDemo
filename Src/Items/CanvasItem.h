#ifndef CANVASITEM_H
#define CANVASITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsRectItem>

// 画布图元 — 纯白背景板，左上角在 (0,0)，不可选择/移动/删除
class CanvasItem : public QGraphicsRectItem
{
public:
    enum { Type = UserType + 100 }; // 特殊类型，不参与 IGraphicsItem 枚举

    explicit CanvasItem(QGraphicsItem *parent = nullptr);
    CanvasItem(const QSizeF &sizePx, QGraphicsItem *parent = nullptr);
    // 以物理 mm 构造画布
    CanvasItem(qreal widthMm, qreal heightMm, qreal displayPpi = 300.0,
               QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    void setCanvasSize(const QSizeF &sizePx);
    QSizeF canvasSize() const;

    // 画布物理尺寸 (mm)
    void setCanvasSizeMm(qreal wMm, qreal hMm);
    qreal canvasWidthMm() const { return m_physicalWidthMm; }
    qreal canvasHeightMm() const { return m_physicalHeightMm; }

    // 显示 PPI（用于 mm↔px 换算，仅供参考，不锁定）
    qreal displayPpi() const { return m_ppi; }
    void setDisplayPpi(qreal ppi);
    void setPpi(qreal ppi);

    // 1mm 对应的场景像素数（基于当前显示 PPI）
    qreal pixelsPerMm() const;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

private:
    void updatePixelRect();

    qreal m_ppi = 300.0;              // 显示 PPI（mm↔px 换算）
    qreal m_physicalWidthMm = 210.0;  // 画布物理宽 (mm)
    qreal m_physicalHeightMm = 297.0; // 画布物理高 (mm)
};

#endif // CANVASITEM_H
