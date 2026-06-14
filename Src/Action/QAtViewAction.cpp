#include "QAtViewAction.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"

QAtGraphicsView* QAtViewActionBase::view() const
{
    auto *p = AppContext::get().activeCanvasPage();
    return p ? p->view() : nullptr;
}
