#include "ArrangeActions.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"
#include "qatgraphicsview.h"
#include "IGraphicsItem.h"
#include "GraphicsItemGroup.h"
#include "Commands/Commands.h"

#include <QGraphicsScene>
#include <QUndoStack>
#include <QGraphicsItem>
#include <QLineF>

// ==================== BringToFrontAction ====================
QIcon BringToFrontAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/bring-front.svg"));
}
QString BringToFrontAction::_text()
{
    return QObject::tr("Bring to &Front");
}
QKeySequence BringToFrontAction::_shortcut() const
{
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up);
}
void BringToFrontAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene())
        return;
    auto items = page->scene()->selectedItems();
    if (items.isEmpty())
        return;

    QList<qreal> oldZ, newZ;
    for (auto *item : items) {
        oldZ << item->zValue();
        newZ << item->zValue() + 1.0;
    }
    page->undoStack()->push(new ZValueChangeCommand(items, oldZ, newZ, page->scene()));
}

// ==================== SendToBackAction ====================
QIcon SendToBackAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/send-back.svg"));
}
QString SendToBackAction::_text()
{
    return QObject::tr("Send to &Back");
}
QKeySequence SendToBackAction::_shortcut() const
{
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down);
}
void SendToBackAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene())
        return;
    auto items = page->scene()->selectedItems();
    if (items.isEmpty())
        return;

    QList<qreal> oldZ, newZ;
    for (auto *item : items) {
        oldZ << item->zValue();
        newZ << item->zValue() - 1.0;
    }
    page->undoStack()->push(new ZValueChangeCommand(items, oldZ, newZ, page->scene()));
}

// ==================== GroupAction ====================
QIcon GroupAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/group.svg"));
}
QString GroupAction::_text()
{
    return QObject::tr("&Group");
}
QKeySequence GroupAction::_shortcut() const
{
    return QKeySequence(Qt::CTRL | Qt::Key_G);
}
void GroupAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene())
        return;
    auto items = ::filterSelectableItems(page->scene()->selectedItems());
    if (items.size() < 2)
        return;

    // Filter out already-grouped children
    QList<QGraphicsItem *> topLevel;
    for (auto *item : items)
        if (!item->parentItem())
            topLevel << item;
    if (topLevel.size() < 2)
        return;

    page->undoStack()->push(new GroupItemsCommand(page->scene(), topLevel));
}

// ==================== UngroupAction ====================
QIcon UngroupAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/ungroup.svg"));
}
QString UngroupAction::_text()
{
    return QObject::tr("&Ungroup");
}
QKeySequence UngroupAction::_shortcut() const
{
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G);
}
void UngroupAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene())
        return;
    auto items = ::filterSelectableItems(page->scene()->selectedItems());
    if (items.size() != 1)
        return;

    auto *group = dynamic_cast<GraphicsItemGroup *>(items.first());
    if (!group)
        return;

    page->undoStack()->push(new UngroupItemsCommand(page->scene(), group));
}

// ==================== FitCanvasToItemsAction ====================
QIcon FitCanvasToItemsAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/view-fit.svg"));
}
QString FitCanvasToItemsAction::_text()
{
    return QObject::tr("Fit Canvas to &Items");
}
void FitCanvasToItemsAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene())
        return;
    QGraphicsScene *sc = page->scene();
    if (sc->items().isEmpty())
        return;

    QRectF bounds = sc->itemsBoundingRect();
    if (bounds.isEmpty())
        return;

    // Add margins
    QSizeF newSize = bounds.size() + QSizeF(20, 20);
    page->setCanvasSize(newSize);
}

// ==================== Rotate helper ====================
static void rotateSelectedItemsBy(qreal angleDelta)
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene() || !page->undoStack())
        return;
    auto *sc = page->scene();
    auto *us = page->undoStack();
    auto *view = page->view();

    auto items = ::filterSelectableItems(sc->selectedItems());
    if (items.isEmpty())
        return;

    QList<QGraphicsItem *> rotatableItems;
    for (auto *item : items) {
        auto *igi = dynamic_cast<IGraphicsItem *>(item);
        if (igi && (igi->propertyFlags() & IGraphicsItem::HasRotation))
            rotatableItems << item;
    }
    if (rotatableItems.isEmpty())
        return;

    items = rotatableItems;

    if (items.size() == 1) {
        auto *item = items.first();
        qreal oldRotation = item->rotation();
        qreal newRotation = oldRotation + angleDelta;
        us->push(new RotationChangeCommand(item, oldRotation, newRotation, sc));
    } else {
        QRectF groupSceneRect;
        for (auto *item : items) {
            QRectF itemSceneRect = item->mapToScene(item->boundingRect()).boundingRect();
            groupSceneRect = groupSceneRect.united(itemSceneRect);
        }
        QPointF groupCenter = groupSceneRect.center();

        us->beginMacro(QObject::tr("Rotate %1°").arg(angleDelta, 0, 'f', 0));

        QList<QGraphicsItem *> moveItems;
        QList<QPointF> oldPositions;
        QList<QPointF> newPositions;

        for (auto *item : items) {
            qreal oldRotation = item->rotation();
            qreal newRotation = oldRotation + angleDelta;
            us->push(new RotationChangeCommand(item, oldRotation, newRotation, sc));

            QPointF posAfterCenterComp = item->pos();
            QPointF currentCenter = item->mapToScene(item->boundingRect().center());
            QLineF line(groupCenter, currentCenter);
            line.setAngle(line.angle() + angleDelta);
            QPointF orbitedCenter = line.p2();
            QPointF orbitalDelta = orbitedCenter - currentCenter;
            item->setPos(item->pos() + orbitalDelta);

            moveItems << item;
            oldPositions << posAfterCenterComp;
            newPositions << item->pos();
        }

        if (!moveItems.isEmpty())
            us->push(new MoveItemsCommand(moveItems, oldPositions, newPositions, sc));

        us->endMacro();
    }

    if (view)
        view->scheduleResizeHandleUpdate();
}

// ==================== RotateCWAction ====================
QIcon RotateCWAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/rotate-cw.svg"));
}
QString RotateCWAction::_text()
{
    return QObject::tr("Rotate 90° CW");
}
void RotateCWAction::_execute()
{
    rotateSelectedItemsBy(90.0);
}

// ==================== RotateCCWAction ====================
QIcon RotateCCWAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/rotate-ccw.svg"));
}
QString RotateCCWAction::_text()
{
    return QObject::tr("Rotate 90° CCW");
}
void RotateCCWAction::_execute()
{
    rotateSelectedItemsBy(-90.0);
}

// ==================== Rotate180Action ====================
QString Rotate180Action::_text()
{
    return QObject::tr("Rotate 180°");
}
void Rotate180Action::_execute()
{
    rotateSelectedItemsBy(180.0);
}

// ==================== AutoLayoutAction ====================
QIcon AutoLayoutAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/auto-layout.svg"));
}
QString AutoLayoutAction::_text()
{
    return QObject::tr("Auto &Layout");
}
void AutoLayoutAction::_execute()
{
    AppContext::get().invokeActionCallback(token());
}
