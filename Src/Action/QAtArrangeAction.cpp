#include "QAtArrangeAction.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"
#include "IGraphicsItem.h"

#include <QGraphicsScene>
#include <QUndoStack>

QGraphicsScene* QAtArrangeActionBase::scene() const
{
    auto *p = AppContext::get().activeCanvasPage();
    return p ? p->scene() : nullptr;
}

QUndoStack* QAtArrangeActionBase::undoStack() const
{
    auto *p = AppContext::get().activeCanvasPage();
    return p ? p->undoStack() : nullptr;
}

QAtGraphicsView* QAtArrangeActionBase::view() const
{
    auto *p = AppContext::get().activeCanvasPage();
    return p ? p->view() : nullptr;
}

QList<QGraphicsItem*> QAtArrangeActionBase::selectedItems() const
{
    auto *p = AppContext::get().activeCanvasPage();
    if (!p || !p->scene()) return {};
    return ::filterSelectableItems(p->scene()->selectedItems());
}
