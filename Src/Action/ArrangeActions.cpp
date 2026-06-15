#include "ArrangeActions.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"
#include "IGraphicsItem.h"
#include "GraphicsItemGroup.h"
#include "Commands/Commands.h"

#include <QGraphicsScene>
#include <QUndoStack>
#include <QGraphicsItem>

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
    AppContext::get().invokeActionCallback(token());
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
    AppContext::get().invokeActionCallback(token());
}

// ==================== Rotate180Action ====================
QString Rotate180Action::_text()
{
    return QObject::tr("Rotate 180°");
}
void Rotate180Action::_execute()
{
    AppContext::get().invokeActionCallback(token());
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
