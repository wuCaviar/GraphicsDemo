#include "ActionContext.h"
#include "AppContext.h"

ActionContext::ActionContext(QObject *parent) : QObject(parent) {}

QAtPage* ActionContext::activePage() const
{
    return AppContext::get().activePage();
}

QAtCanvasPage* ActionContext::activeCanvasPage() const
{
    return AppContext::get().activeCanvasPage();
}

ClipboardService* ActionContext::clipboard() const { return AppContext::get().clipboard(); }
ThemeService*     ActionContext::theme() const     { return AppContext::get().theme(); }
ProgressService*  ActionContext::progress() const   { return AppContext::get().progress(); }
NetworkService*   ActionContext::network() const    { return AppContext::get().network(); }
ProcessService*   ActionContext::process() const    { return AppContext::get().process(); }
ProjectService*   ActionContext::project() const    { return AppContext::get().project(); }
UndoService*      ActionContext::undo() const       { return AppContext::get().undo(); }

void ActionContext::setMaybeSaveProject(SaveCallback cb)
{
    AppContext::get().setMaybeSaveProject(std::move(cb));
}

bool ActionContext::maybeSaveProject()
{
    return AppContext::get().maybeSaveProject();
}
