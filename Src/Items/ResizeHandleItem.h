#ifndef RESIZEHANDLEITEM_H
#define RESIZEHANDLEITEM_H

#include <QGraphicsItem>
#include <QHash>
#include <QMap>
#include <QSet>

class QUndoStack;

// 缩放手柄装饰器 — 包裹在选中图元周围，提供8个缩放手柄
// 支持单目标模式和组模式（多选包围框）
class ResizeHandleItem : public QGraphicsItem
{
public:
    enum HandleRole {
        NoHandle = 0,
        TopLeft, Top, TopRight,
        Left, Right,
        BottomLeft, Bottom, BottomRight
    };

    enum { Type = UserType + 200 };

    explicit ResizeHandleItem(QGraphicsItem *target = nullptr, QGraphicsItem *parent = nullptr);
    ~ResizeHandleItem();

    int type() const override { return Type; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // 单目标模式
    void setTargetItem(QGraphicsItem *target);
    QGraphicsItem *targetItem() const { return m_target; }

    // 组模式
    void setTargetItems(const QList<QGraphicsItem *> &items);
    QList<QGraphicsItem *> targetItems() const { return m_targetItems; }
    bool isGroupMode() const { return !m_targetItems.isEmpty(); }

    void updateHandlePositions();
    void setUndoStack(QUndoStack *stack) { m_undoStack = stack; }

    // 粘贴样式（区分复制图元）
    void setPastedStyle(bool pasted);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    struct HandleInfo {
        QRectF rect;
        HandleRole role;
    };

    HandleRole handleAtPos(const QPointF &scenePos) const;
    Qt::CursorShape cursorForRole(HandleRole role) const;
    void applyResize(HandleRole role, const QPointF &scenePos);
    void applyGroupResize(HandleRole role, const QPointF &scenePos);
    QRectF computeGroupBoundingRect() const;

    // 单目标模式
    QGraphicsItem *m_target = nullptr;

    // 组模式
    QList<QGraphicsItem *> m_targetItems;
    QRectF m_groupRect;
    QMap<QGraphicsItem *, QRectF> m_originalItemRects;
    QMap<QGraphicsItem *, QPointF> m_originalItemPositions;
    QRectF m_originalGroupRect;

    // 通用状态
    QVector<HandleInfo> m_handles;
    HandleRole m_activeRole = NoHandle;
    HandleRole m_hoverRole = NoHandle;
    QPointF m_pressPos;
    QRectF m_originalRect;
    QPointF m_originalPos;
    QUndoStack *m_undoStack = nullptr;
    bool m_pastedStyle = false;

    // 记录 resize 前的几何状态，用于 undo
    QRectF m_preResizeRect;
    QPointF m_preResizePos;

    static const int kHandleSize = 6;
};

#endif // RESIZEHANDLEITEM_H
