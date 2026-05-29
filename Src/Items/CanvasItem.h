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
    CanvasItem(const QSizeF &size, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    void setCanvasSize(const QSizeF &size);
    QSizeF canvasSize() const;

    void setPpi(qreal ppi);
    qreal ppi() const { return m_ppi; }
    bool hasDpi() const { return m_ppi > 0.0; }

    // 1mm 对应的场景像素数（无 DPI 时返回 1.0，即 1mm = 1 scene unit）
    qreal pixelsPerMm() const { return hasDpi() ? (m_ppi / 25.4) : 1.0; }

    // mm 辅助方法
    void setCanvasSizeMm(const QSizeF &sizeMm);
    QSizeF canvasSizeMm() const;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

private:
    qreal m_ppi = 0.0; // 0 表示尚未由图片确定 DPI
};

#endif // CANVASITEM_H
