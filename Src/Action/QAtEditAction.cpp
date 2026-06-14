#include "QAtEditAction.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"

#include <QGraphicsScene>
#include <QUndoStack>

QGraphicsScene* QAtEditActionBase::scene() const
{
    auto *p = AppContext::get().activeCanvasPage();
    return p ? p->scene() : nullptr;
}

QUndoStack* QAtEditActionBase::undoStack() const
{
    auto *p = AppContext::get().activeCanvasPage();
    return p ? p->undoStack() : nullptr;
}

QAtGraphicsView* QAtEditActionBase::view() const
{
    auto *p = AppContext::get().activeCanvasPage();
    return p ? p->view() : nullptr;
}
