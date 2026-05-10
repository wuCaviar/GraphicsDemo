#include "ResizeHandleItem.h"

#include "RectItem.h"
#include "EllipseItem.h"
#include "ImageItem.h"
#include "TextItem.h"
#include "CanvasItem.h"
#include "Commands.h"

#include <QBrush>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QUndoStack>

const int ResizeHandleItem::kHandleSize;

ResizeHandleItem::ResizeHandleItem(QGraphicsItem *target, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_target(target)
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setZValue(9999); // 确保在最顶层
    setAcceptHoverEvents(true);
    updateHandlePositions();
}

ResizeHandleItem::~ResizeHandleItem() {}

QRectF ResizeHandleItem::boundingRect() const
{
    if (isGroupMode()) {
        return m_groupRect.adjusted(-kHandleSize, -kHandleSize, kHandleSize, kHandleSize);
    }

    QRectF r;
    if (m_target)
        r = m_target->boundingRect().adjusted(-kHandleSize, -kHandleSize, kHandleSize, kHandleSize);
    return r;
}

void ResizeHandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QRectF br;
    if (isGroupMode()) {
        br = m_groupRect;
    } else if (m_target) {
        br = m_target->boundingRect();
    } else {
        return;
    }

    // 绘制选中虚线框 — 粘贴样式用 DotLine，普通样式用 DashLine
    Qt::PenStyle style = m_pastedStyle ? Qt::DotLine : Qt::DashLine;
    painter->setPen(QPen(Qt::blue, 1, style));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(br);

    // 绘制手柄
    painter->setPen(QPen(Qt::blue, 1));
    painter->setBrush(QBrush(Qt::white));
    for (const auto &h : m_handles) {
        painter->drawRect(h.rect);
    }
}

void ResizeHandleItem::updateHandlePositions()
{
    m_handles.clear();
    prepareGeometryChange();

    if (isGroupMode()) {
        // 组模式：计算所有图元的包围矩形（场景坐标），ResizeHandleItem 位于原点
        m_groupRect = computeGroupBoundingRect();
        setPos(0, 0);
        setRotation(0);

        QRectF br = m_groupRect;
        qreal hs = kHandleSize / 2.0;
        m_handles = {
            { QRectF(br.left() - hs, br.top() - hs, kHandleSize, kHandleSize), TopLeft },
            { QRectF(br.center().x() - hs, br.top() - hs, kHandleSize, kHandleSize), Top },
            { QRectF(br.right() - hs, br.top() - hs, kHandleSize, kHandleSize), TopRight },
            { QRectF(br.left() - hs, br.center().y() - hs, kHandleSize, kHandleSize), Left },
            { QRectF(br.right() - hs, br.center().y() - hs, kHandleSize, kHandleSize), Right },
            { QRectF(br.left() - hs, br.bottom() - hs, kHandleSize, kHandleSize), BottomLeft },
            { QRectF(br.center().x() - hs, br.bottom() - hs, kHandleSize, kHandleSize), Bottom },
            { QRectF(br.right() - hs, br.bottom() - hs, kHandleSize, kHandleSize), BottomRight },
        };
    } else if (m_target) {
        // 单目标模式：同步位置和旋转到目标图元，使本地坐标系对齐
        setPos(m_target->pos());
        setRotation(m_target->rotation());

        QRectF br = m_target->boundingRect();
        qreal hs = kHandleSize / 2.0;
        m_handles = {
            { QRectF(br.left() - hs, br.top() - hs, kHandleSize, kHandleSize), TopLeft },
            { QRectF(br.center().x() - hs, br.top() - hs, kHandleSize, kHandleSize), Top },
            { QRectF(br.right() - hs, br.top() - hs, kHandleSize, kHandleSize), TopRight },
            { QRectF(br.left() - hs, br.center().y() - hs, kHandleSize, kHandleSize), Left },
            { QRectF(br.right() - hs, br.center().y() - hs, kHandleSize, kHandleSize), Right },
            { QRectF(br.left() - hs, br.bottom() - hs, kHandleSize, kHandleSize), BottomLeft },
            { QRectF(br.center().x() - hs, br.bottom() - hs, kHandleSize, kHandleSize), Bottom },
            { QRectF(br.right() - hs, br.bottom() - hs, kHandleSize, kHandleSize), BottomRight },
        };
    }

    update();
}

void ResizeHandleItem::setTargetItem(QGraphicsItem *target)
{
    m_target = target;
    m_targetItems.clear();
    updateHandlePositions();
}

void ResizeHandleItem::setTargetItems(const QList<QGraphicsItem *> &items)
{
    m_target = nullptr;
    m_targetItems = items;
    updateHandlePositions();
}

void ResizeHandleItem::setPastedStyle(bool pasted)
{
    m_pastedStyle = pasted;
    update();
}

QRectF ResizeHandleItem::computeGroupBoundingRect() const
{
    QRectF result;
    for (auto *item : m_targetItems) {
        if (item->type() == CanvasItem::Type)
            continue;
        QRectF itemSceneRect = item->mapToScene(item->boundingRect()).boundingRect();
        result = result.united(itemSceneRect);
    }
    return result;
}

ResizeHandleItem::HandleRole ResizeHandleItem::handleAtPos(const QPointF &scenePos) const
{
    QPointF localPos = mapFromScene(scenePos);
    for (const auto &h : m_handles) {
        if (h.rect.contains(localPos))
            return h.role;
    }
    return NoHandle;
}

Qt::CursorShape ResizeHandleItem::cursorForRole(HandleRole role) const
{
    switch (role) {
    case TopLeft:
    case BottomRight: return Qt::SizeFDiagCursor;
    case TopRight:
    case BottomLeft: return Qt::SizeBDiagCursor;
    case Top:
    case Bottom: return Qt::SizeVerCursor;
    case Left:
    case Right: return Qt::SizeHorCursor;
    default: return Qt::ArrowCursor;
    }
}

// ============================================================
// Hover 事件 — 显示对应方向的缩放光标
// ============================================================
void ResizeHandleItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    HandleRole role = handleAtPos(event->scenePos());
    if (role != m_hoverRole) {
        m_hoverRole = role;
        if (role != NoHandle)
            setCursor(cursorForRole(role));
        else
            unsetCursor();
    }
    event->accept();
}

void ResizeHandleItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hoverRole = NoHandle;
    unsetCursor();
    event->accept();
}

// ============================================================
// 鼠标事件
// ============================================================
void ResizeHandleItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    HandleRole role = handleAtPos(event->scenePos());
    if (role == NoHandle) {
        event->ignore();
        return;
    }

    m_activeRole = role;
    m_pressPos = event->scenePos();

    if (isGroupMode()) {
        // 组模式：记录组包围框和每个图元的原始状态
        m_originalGroupRect = m_groupRect;
        m_originalItemRects.clear();
        m_originalItemPositions.clear();
        for (auto *item : m_targetItems) {
            if (item->type() == CanvasItem::Type)
                continue;
            m_originalItemPositions[item] = item->pos();
            if (auto *ri = qgraphicsitem_cast<RectItem *>(item)) {
                m_originalItemRects[item] = ri->rect();
            } else if (auto *ei = qgraphicsitem_cast<EllipseItem *>(item)) {
                m_originalItemRects[item] = ei->rect();
            } else if (auto *ti = qgraphicsitem_cast<TextItem *>(item)) {
                m_originalItemRects[item] = ti->rect();
            } else if (auto *ii = qgraphicsitem_cast<ImageItem *>(item)) {
                m_originalItemRects[item] = ii->rect();
            } else {
                m_originalItemRects[item] = item->boundingRect();
            }
        }
    } else {
        // 单目标模式：记录 resize 前的几何状态
        m_preResizePos = m_target->pos();

        if (auto *ri = qgraphicsitem_cast<RectItem *>(m_target)) {
            m_preResizeRect = ri->rect();
        } else if (auto *ei = qgraphicsitem_cast<EllipseItem *>(m_target)) {
            m_preResizeRect = ei->rect();
        } else if (auto *ti = qgraphicsitem_cast<TextItem *>(m_target)) {
            m_preResizeRect = ti->rect();
        } else if (auto *ii = qgraphicsitem_cast<ImageItem *>(m_target)) {
            m_preResizeRect = ii->rect();
        } else {
            m_preResizeRect = m_target->boundingRect();
        }

        m_originalRect = m_preResizeRect;
        m_originalPos = m_preResizePos;
    }

    event->accept();
}

void ResizeHandleItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeRole == NoHandle) {
        event->ignore();
        return;
    }

    if (isGroupMode()) {
        applyGroupResize(m_activeRole, event->scenePos());
    } else if (m_target) {
        applyResize(m_activeRole, event->scenePos());
    }

    updateHandlePositions();
    event->accept();
}

void ResizeHandleItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeRole == NoHandle) {
        event->accept();
        return;
    }

    if (isGroupMode() && m_undoStack) {
        // 组模式 undo：使用 macro 合并所有图元的变更
        m_undoStack->beginMacro(QStringLiteral("Group Resize"));

        for (auto *item : m_targetItems) {
            if (item->type() == CanvasItem::Type)
                continue;

            QPointF newPos = item->pos();
            QPointF oldPos = m_originalItemPositions.value(item, item->pos());
            QRectF oldRect = m_originalItemRects.value(item);

            // 位置变更
            if (oldPos != newPos) {
                m_undoStack->push(new MoveItemsCommand({item}, {oldPos}, {newPos},
                                                        scene()));
            }

            // 几何变更（RectItem / EllipseItem / TextItem / ImageItem）
            QRectF newRect;
            if (auto *ri = qgraphicsitem_cast<RectItem *>(item)) {
                newRect = ri->rect();
            } else if (auto *ei = qgraphicsitem_cast<EllipseItem *>(item)) {
                newRect = ei->rect();
            } else if (auto *ti = qgraphicsitem_cast<TextItem *>(item)) {
                newRect = ti->rect();
            } else if (auto *ii = qgraphicsitem_cast<ImageItem *>(item)) {
                newRect = ii->rect();
            }

            if (newRect.isValid() && oldRect.isValid() && oldRect != newRect) {
                m_undoStack->push(new PropertyChangeCommand(
                        item, PropertyChangeCommand::Geometry,
                        QVariant(oldRect), QVariant(newRect), scene()));
            }
        }

        m_undoStack->endMacro();
    } else if (m_target && m_undoStack) {
        // 单目标模式 undo
        QRectF newRect;
        QPointF newPos = m_target->pos();

        if (auto *ri = qgraphicsitem_cast<RectItem *>(m_target)) {
            newRect = ri->rect();
        } else if (auto *ei = qgraphicsitem_cast<EllipseItem *>(m_target)) {
            newRect = ei->rect();
        } else if (auto *ti = qgraphicsitem_cast<TextItem *>(m_target)) {
            newRect = ti->rect();
        } else if (auto *ii = qgraphicsitem_cast<ImageItem *>(m_target)) {
            newRect = ii->rect();
        } else {
            newRect = m_target->boundingRect();
        }

        if (m_preResizeRect != newRect || m_preResizePos != newPos) {
            if (dynamic_cast<IGraphicsItem *>(m_target)) {
                m_undoStack->push(new PropertyChangeCommand(
                        m_target, PropertyChangeCommand::Geometry,
                        QVariant(m_preResizeRect), QVariant(newRect), scene()));
            }
        }
    }

    m_activeRole = NoHandle;
    event->accept();
}

// ============================================================
// 单目标缩放
// ============================================================
void ResizeHandleItem::applyResize(HandleRole role, const QPointF &scenePos)
{
    if (!m_target) return;

    // 将场景坐标的 delta 转换到 ResizeHandleItem 本地坐标
    // （ResizeHandleItem 与目标共享位置和旋转，因此等价于目标的本地坐标）
    QPointF delta = mapFromScene(scenePos) - mapFromScene(m_pressPos);

    qreal left = m_originalRect.left();
    qreal top = m_originalRect.top();
    qreal right = m_originalRect.right();
    qreal bottom = m_originalRect.bottom();

    switch (role) {
    case TopLeft:     left += delta.x(); top += delta.y(); break;
    case Top:         top += delta.y(); break;
    case TopRight:    right += delta.x(); top += delta.y(); break;
    case Left:        left += delta.x(); break;
    case Right:       right += delta.x(); break;
    case BottomLeft:  left += delta.x(); bottom += delta.y(); break;
    case Bottom:      bottom += delta.y(); break;
    case BottomRight: right += delta.x(); bottom += delta.y(); break;
    default: return;
    }

    // 保证最小尺寸
    if (right - left < 10) {
        if (role == TopLeft || role == Left || role == BottomLeft)
            left = right - 10;
        else
            right = left + 10;
    }
    if (bottom - top < 10) {
        if (role == TopLeft || role == Top || role == TopRight)
            top = bottom - 10;
        else
            bottom = top + 10;
    }

    QRectF newRect(left, top, right - left, bottom - top);

    if (auto *ri = qgraphicsitem_cast<RectItem *>(m_target)) {
        ri->setRect(newRect);
    } else if (auto *ei = qgraphicsitem_cast<EllipseItem *>(m_target)) {
        ei->setRect(newRect);
    } else if (auto *ti = qgraphicsitem_cast<TextItem *>(m_target)) {
        // TextItem 通过 setRect() 实现尺寸变更
        ti->setRect(newRect);
    } else if (auto *ii = qgraphicsitem_cast<ImageItem *>(m_target)) {
        // ImageItem 通过 setRect() 实现尺寸变更
        ii->setRect(newRect);
    } else {
        // 其他图元通过 scale 缩放（兜底）
        QSizeF originalSize = m_originalRect.size();
        if (originalSize.width() > 0 && originalSize.height() > 0) {
            qreal sx = newRect.width() / originalSize.width();
            qreal sy = newRect.height() / originalSize.height();
            QTransform t;
            t.scale(sx, sy);
            m_target->setTransform(t);
        }
    }
}

// ============================================================
// 组模式缩放 — 按比例缩放所有图元
// ============================================================
void ResizeHandleItem::applyGroupResize(HandleRole role, const QPointF &scenePos)
{
    QPointF delta = scenePos - m_pressPos;

    qreal left = m_originalGroupRect.left();
    qreal top = m_originalGroupRect.top();
    qreal right = m_originalGroupRect.right();
    qreal bottom = m_originalGroupRect.bottom();

    switch (role) {
    case TopLeft:     left += delta.x(); top += delta.y(); break;
    case Top:         top += delta.y(); break;
    case TopRight:    right += delta.x(); top += delta.y(); break;
    case Left:        left += delta.x(); break;
    case Right:       right += delta.x(); break;
    case BottomLeft:  left += delta.x(); bottom += delta.y(); break;
    case Bottom:      bottom += delta.y(); break;
    case BottomRight: right += delta.x(); bottom += delta.y(); break;
    default: return;
    }

    // 保证最小尺寸
    if (right - left < 20) {
        if (role == TopLeft || role == Left || role == BottomLeft)
            left = right - 20;
        else
            right = left + 20;
    }
    if (bottom - top < 20) {
        if (role == TopLeft || role == Top || role == TopRight)
            top = bottom - 20;
        else
            bottom = top + 20;
    }

    QRectF newGroupRect(left, top, right - left, bottom - top);

    // 计算缩放因子
    qreal sx = (newGroupRect.width() > 0 && m_originalGroupRect.width() > 0)
                   ? newGroupRect.width() / m_originalGroupRect.width()
                   : 1.0;
    qreal sy = (newGroupRect.height() > 0 && m_originalGroupRect.height() > 0)
                   ? newGroupRect.height() / m_originalGroupRect.height()
                   : 1.0;

    QPointF groupCenter = m_originalGroupRect.center();

    // 按比例调整每个图元
    for (auto *item : m_targetItems) {
        if (item->type() == CanvasItem::Type)
            continue;

        QPointF origPos = m_originalItemPositions.value(item, item->pos());
        QRectF origRect = m_originalItemRects.value(item, item->boundingRect());

        // 图元中心相对于组中心的偏移
        qreal origCenterX = origPos.x() + origRect.center().x();
        qreal origCenterY = origPos.y() + origRect.center().y();

        // 新中心 = 组中心 + (原中心 - 组中心) * 缩放因子
        qreal newCenterX = groupCenter.x() + (origCenterX - groupCenter.x()) * sx;
        qreal newCenterY = groupCenter.y() + (origCenterY - groupCenter.y()) * sy;

        // 缩放图元尺寸
        QSizeF scaledSize(origRect.width() * sx, origRect.height() * sy);

        if (auto *ri = qgraphicsitem_cast<RectItem *>(item)) {
            QRectF scaledRect(ri->rect().topLeft(), scaledSize);
            ri->setRect(scaledRect);
            // 新位置 = 新中心 - 缩放后 rect 的本地中心偏移
            QPointF newPos(newCenterX - scaledRect.center().x(),
                           newCenterY - scaledRect.center().y());
            item->setPos(newPos);
        } else if (auto *ei = qgraphicsitem_cast<EllipseItem *>(item)) {
            QRectF scaledRect(ei->rect().topLeft(), scaledSize);
            ei->setRect(scaledRect);
            QPointF newPos(newCenterX - scaledRect.center().x(),
                           newCenterY - scaledRect.center().y());
            item->setPos(newPos);
        } else if (auto *ti = qgraphicsitem_cast<TextItem *>(item)) {
            QRectF scaledRect(origRect.topLeft(), scaledSize);
            ti->setRect(scaledRect);
            QPointF newPos(newCenterX - scaledRect.center().x(),
                           newCenterY - scaledRect.center().y());
            item->setPos(newPos);
        } else if (auto *ii = qgraphicsitem_cast<ImageItem *>(item)) {
            QRectF scaledRect(origRect.topLeft(), scaledSize);
            ii->setRect(scaledRect);
            QPointF newPos(newCenterX - scaledRect.center().x(),
                           newCenterY - scaledRect.center().y());
            item->setPos(newPos);
        } else {
            // 其他图元通过 transform 缩放
            QSizeF origSize = origRect.size();
            QPointF newPos(newCenterX - scaledSize.width() / 2.0,
                           newCenterY - scaledSize.height() / 2.0);
            if (origSize.width() > 0 && origSize.height() > 0) {
                qreal itemSx = scaledSize.width() / origSize.width();
                qreal itemSy = scaledSize.height() / origSize.height();
                item->setTransform(QTransform().scale(itemSx, itemSy));
            }
            item->setPos(newPos);
        }
    }
}
