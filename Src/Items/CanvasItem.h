#ifndef CANVASITEM_H
#define CANVASITEM_H

#include "IGraphicsItem.h"
#include <QGraphicsRectItem>

// 画布图元 — 纯白背景板，左上角在 (0,0)，不可选择/移动/删除
class CanvasItem : public QGraphicsRectItem
{
public:
    enum
    {
        Type = UserType + 100
    }; // 特殊类型，不参与 IGraphicsItem 枚举

    explicit CanvasItem(QGraphicsItem *parent = nullptr);
    CanvasItem(const QSizeF &size, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    void setCanvasSize(const QSizeF &size);
    QSizeF canvasSize() const;

    // 画布 DPI（由导入图片确定）
    void setCanvasDpi(int dpiX, int dpiY);
    int canvasDpiX() const { return m_canvasDpiX; }
    int canvasDpiY() const { return m_canvasDpiY; }
    bool isDpiLocked() const { return m_dpiLocked; }
    void lockDpi();
    void unlockDpi();

    // 兼容旧接口：设置显示用 PPI（内部用于标尺/状态栏换算）
    void setPpi(qreal ppi);
    qreal ppi() const { return m_ppi; }

    // 1mm 对应的场景像素数（基于当前有效 PPI）
    qreal pixelsPerMm() const;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void updateEffectivePpi();

    qreal m_ppi = 300.0; // 有效显示 PPI（用于 mm↔px 换算）
    int m_canvasDpiX = 0; // 画布 X 方向 DPI，0 = 未确定
    int m_canvasDpiY = 0; // 画布 Y 方向 DPI，0 = 未确定
    bool m_dpiLocked = false; // DPI 是否锁定
};

#endif // CANVASITEM_H
