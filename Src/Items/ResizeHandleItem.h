#ifndef RESIZEHANDLEITEM_H
#define RESIZEHANDLEITEM_H

#include <QGraphicsItem>
#include <QMap>
#include <QPolygon>

class QUndoStack;

// 缩放手柄装饰器 — 在选中图元周围提供8个缩放手柄
// 始终位于场景原点 (0,0)，rotation=0 — 统一使用场景坐标
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

    explicit ResizeHandleItem(QGraphicsItem *parent = nullptr);
    ~ResizeHandleItem();

    int type() const override { return Type; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setTargetItem(QGraphicsItem *target);
    QGraphicsItem *targetItem() const { return m_target; }

    void setTargetItems(const QList<QGraphicsItem *> &items);
    QList<QGraphicsItem *> targetItems() const { return m_targetItems; }
    bool isGroupMode() const { return !m_targetItems.isEmpty(); }

    void updateHandlePositions();
    void setUndoStack(QUndoStack *stack) { m_undoStack = stack; }

    void setPastedStyle(bool pasted);

    bool isResizing() const { return m_activeRole != NoHandle; }
    bool isTargetValid() const;

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

    // 几何工具
    HandleRole handleAtPos(const QPointF &scenePos) const;
    Qt::CursorShape cursorForRole(HandleRole role) const;
    void applyResize(HandleRole role, const QPointF &scenePos);
    void applyGroupResize(HandleRole role, const QPointF &scenePos);
    QPolygonF computeSelectionPolygon() const;
    QRectF computeGroupBoundingRect() const;

    // 目标
    QGraphicsItem *m_target = nullptr;
    QList<QGraphicsItem *> m_targetItems;

    // 选中框多边形（场景坐标，可能为旋转矩形的4个顶点）
    QPolygonF m_selectionPolygon;

    // 组缩放 undo 状态
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

    // 单目标 resize undo 状态
    QRectF m_preResizeRect;
    QPointF m_preResizePos;

    static const int kHandleSize = 6;
};

#endif // RESIZEHANDLEITEM_H
