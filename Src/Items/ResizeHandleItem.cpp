#include "ResizeHandleItem.h"

#include "CanvasItem.h"
#include "Commands.h"
#include "IGraphicsItem.h"

#include <QBrush>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QUndoStack>

const int ResizeHandleItem::kHandleSize;

// ---- helpers using IGraphicsItem interface ----

static QRectF getItemGeometry(QGraphicsItem *item)
{
    auto *igi = dynamic_cast<IGraphicsItem *>(item);
    if (igi && igi->supportsGeometryRect())
        return igi->geometryRect();
    return item->boundingRect();
}

static void setItemGeometry(QGraphicsItem *item, const QRectF &r)
{
    auto *igi = dynamic_cast<IGraphicsItem *>(item);
    if (igi && igi->supportsSetGeometryRect())
        igi->setGeometryRect(r);
}

// ============================================================

ResizeHandleItem::ResizeHandleItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    setZValue(9999);
    setAcceptHoverEvents(true);
}

ResizeHandleItem::~ResizeHandleItem() {}

// ============================================================
// Bounding rect / painting — always in scene coordinates (handle at origin)
// ============================================================

QRectF ResizeHandleItem::boundingRect() const
{
    return m_selectionPolygon.boundingRect()
        .adjusted(-kHandleSize, -kHandleSize, kHandleSize, kHandleSize);
}

void ResizeHandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!isTargetValid())
        return;

    // 选中虚线/点线框
    Qt::PenStyle style = m_pastedStyle ? Qt::DotLine : Qt::DashLine;
    painter->setPen(QPen(Qt::blue, 1, style));
    painter->setBrush(Qt::NoBrush);
    painter->drawPolygon(m_selectionPolygon);

    // 8 个缩放手柄
    painter->setPen(QPen(Qt::blue, 1));
    painter->setBrush(Qt::white);
    for (const auto &h : m_handles)
        painter->drawRect(h.rect);
}

// ============================================================
// Selection polygon computation (scene coordinates)
// ============================================================

QPolygonF ResizeHandleItem::computeSelectionPolygon() const
{
    QPolygonF poly;
    if (isGroupMode()) {
        QRectF r = computeGroupBoundingRect();
        if (r.isValid()) {
            poly << r.topLeft() << r.topRight()
                 << r.bottomRight() << r.bottomLeft();
        }
    } else if (m_target) {
        QRectF br = m_target->boundingRect();
        poly << m_target->mapToScene(br.topLeft())
             << m_target->mapToScene(br.topRight())
             << m_target->mapToScene(br.bottomRight())
             << m_target->mapToScene(br.bottomLeft());
    }
    return poly;
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

// ============================================================
// 手柄位置更新 — 统一场景坐标，handle 始终位于原点
// ============================================================

void ResizeHandleItem::updateHandlePositions()
{
    m_handles.clear();

    if (!isTargetValid())
        return;

    prepareGeometryChange();

    // 始终位于场景原点
    setPos(0, 0);
    setRotation(0);

    m_selectionPolygon = computeSelectionPolygon();
    if (m_selectionPolygon.size() < 4)
        return;

    const QPointF &tl = m_selectionPolygon[0];
    const QPointF &tr = m_selectionPolygon[1];
    const QPointF &br = m_selectionPolygon[2];
    const QPointF &bl = m_selectionPolygon[3];

    qreal hs = kHandleSize / 2.0;
    auto makeRect = [hs](const QPointF &c) {
        return QRectF(c.x() - hs, c.y() - hs, kHandleSize, kHandleSize);
    };

    m_handles = {
        { makeRect(tl), TopLeft },
        { makeRect((tl + tr) / 2), Top },
        { makeRect(tr), TopRight },
        { makeRect((tl + bl) / 2), Left },
        { makeRect((tr + br) / 2), Right },
        { makeRect(bl), BottomLeft },
        { makeRect((bl + br) / 2), Bottom },
        { makeRect(br), BottomRight },
    };

    update();
}

// ============================================================
// Target management
// ============================================================

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

bool ResizeHandleItem::isTargetValid() const
{
    if (isGroupMode()) {
        return std::all_of(m_targetItems.begin(), m_targetItems.end(),
                           [](QGraphicsItem *item) { return item && item->scene(); });
    }
    return m_target && m_target->scene();
}

// ============================================================
// Hit testing — handle at origin, scene pos == local pos
// ============================================================

ResizeHandleItem::HandleRole ResizeHandleItem::handleAtPos(const QPointF &scenePos) const
{
    for (const auto &h : m_handles) {
        if (h.rect.contains(scenePos))
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
// Hover events
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
// Mouse events
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
        m_originalGroupRect = m_selectionPolygon.boundingRect();
        m_originalItemRects.clear();
        m_originalItemPositions.clear();
        for (auto *item : m_targetItems) {
            if (item->type() == CanvasItem::Type)
                continue;
            m_originalItemPositions[item] = item->pos();
            m_originalItemRects[item] = getItemGeometry(item);
        }
    } else {
        m_preResizePos = m_target->pos();
        m_preResizeRect = getItemGeometry(m_target);
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

    if (!isTargetValid()) {
        m_activeRole = NoHandle;
        event->accept();
        return;
    }

    if (isGroupMode())
        applyGroupResize(m_activeRole, event->scenePos());
    else if (m_target)
        applyResize(m_activeRole, event->scenePos());

    updateHandlePositions();
    event->accept();
}

void ResizeHandleItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeRole == NoHandle) {
        event->accept();
        return;
    }

    if (!isTargetValid()) {
        m_activeRole = NoHandle;
        event->accept();
        return;
    }

    QGraphicsScene *s = scene();
    if (!s) {
        m_activeRole = NoHandle;
        event->accept();
        return;
    }

    if (isGroupMode() && m_undoStack) {
        m_undoStack->beginMacro(QStringLiteral("Group Resize"));

        for (auto *item : m_targetItems) {
            if (item->type() == CanvasItem::Type)
                continue;

            QPointF newPos = item->pos();
            QPointF oldPos = m_originalItemPositions.value(item, item->pos());
            QRectF oldRect = m_originalItemRects.value(item);

            if (oldPos != newPos) {
                m_undoStack->push(new MoveItemsCommand({item}, {oldPos}, {newPos}, s));
            }

            QRectF newRect = getItemGeometry(item);
            if (newRect.isValid() && oldRect.isValid() && oldRect != newRect) {
                m_undoStack->push(new PropertyChangeCommand(
                        item, PropertyChangeCommand::Geometry,
                        QVariant(oldRect), QVariant(newRect), s));
            }
        }

        m_undoStack->endMacro();
    } else if (m_target && m_undoStack) {
        QRectF newRect = getItemGeometry(m_target);
        QPointF newPos = m_target->pos();

        if (m_preResizeRect != newRect || m_preResizePos != newPos) {
            if (dynamic_cast<IGraphicsItem *>(m_target)) {
                m_undoStack->push(new PropertyChangeCommand(
                        m_target, PropertyChangeCommand::Geometry,
                        QVariant(m_preResizeRect), QVariant(newRect), s));
            }
        }
    }

    m_activeRole = NoHandle;
    event->accept();
}

// ============================================================
// 单目标缩放 — delta 通过 mapFromScene 转换到 target 本地坐标系
// ============================================================

void ResizeHandleItem::applyResize(HandleRole role, const QPointF &scenePos)
{
    if (!m_target)
        return;

    QPointF localPress = m_target->mapFromScene(m_pressPos);
    QPointF localCurrent = m_target->mapFromScene(scenePos);
    QPointF delta = localCurrent - localPress;

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

    auto *igi = dynamic_cast<IGraphicsItem *>(m_target);
    if (igi && igi->supportsSetGeometryRect()) {
        setItemGeometry(m_target, newRect);
    } else {
        QSizeF originalSize = m_originalRect.size();
        if (originalSize.width() > 0 && originalSize.height() > 0) {
            qreal sx = newRect.width() / originalSize.width();
            qreal sy = newRect.height() / originalSize.height();
            m_target->setTransform(QTransform().scale(sx, sy));
        }
    }
}

// ============================================================
// 组缩放 — 场景坐标系中按比例缩放所有图元
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

    qreal sx = (newGroupRect.width() > 0 && m_originalGroupRect.width() > 0)
                   ? newGroupRect.width() / m_originalGroupRect.width()
                   : 1.0;
    qreal sy = (newGroupRect.height() > 0 && m_originalGroupRect.height() > 0)
                   ? newGroupRect.height() / m_originalGroupRect.height()
                   : 1.0;

    QPointF groupCenter = m_originalGroupRect.center();

    for (auto *item : m_targetItems) {
        if (item->type() == CanvasItem::Type)
            continue;

        QPointF origPos = m_originalItemPositions.value(item, item->pos());
        QRectF origRect = m_originalItemRects.value(item, item->boundingRect());

        qreal origCenterX = origPos.x() + origRect.center().x();
        qreal origCenterY = origPos.y() + origRect.center().y();
        qreal newCenterX = groupCenter.x() + (origCenterX - groupCenter.x()) * sx;
        qreal newCenterY = groupCenter.y() + (origCenterY - groupCenter.y()) * sy;
        QSizeF scaledSize(origRect.width() * sx, origRect.height() * sy);

        auto *igi = dynamic_cast<IGraphicsItem *>(item);
        if (igi && igi->supportsSetGeometryRect()) {
            QRectF scaledRect(origRect.topLeft(), scaledSize);
            setItemGeometry(item, scaledRect);
            QPointF newPos(newCenterX - scaledRect.center().x(),
                           newCenterY - scaledRect.center().y());
            item->setPos(newPos);
        } else {
            QPointF newPos(newCenterX - scaledSize.width() / 2.0,
                           newCenterY - scaledSize.height() / 2.0);
            QSizeF origSize = origRect.size();
            if (origSize.width() > 0 && origSize.height() > 0) {
                qreal itemSx = scaledSize.width() / origSize.width();
                qreal itemSy = scaledSize.height() / origSize.height();
                item->setTransform(QTransform().scale(itemSx, itemSy));
            }
            item->setPos(newPos);
        }
    }
}
