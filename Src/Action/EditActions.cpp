#include "EditActions.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"
#include "UndoService.h"
#include "ClipboardService.h"
#include "IGraphicsItem.h"
#include "Commands/Commands.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QUndoStack>
#include <QStyle>

// ==================== UndoAction ====================
QIcon UndoAction::_icon() { return QApplication::style()->standardIcon(QStyle::SP_ArrowBack); }
QString UndoAction::_text() { return QObject::tr("&Undo"); }
QKeySequence UndoAction::_shortcut() const { return QKeySequence::Undo; }
void UndoAction::_execute()  { AppContext::get().undo()->undo(); }
void UndoAction::_onUpdateState(bool &enabled, bool &, bool &)
{
    enabled = AppContext::get().undo()->canUndo();
}

// ==================== RedoAction ====================
QIcon RedoAction::_icon() { return QApplication::style()->standardIcon(QStyle::SP_ArrowForward); }
QString RedoAction::_text() { return QObject::tr("&Redo"); }
QKeySequence RedoAction::_shortcut() const { return QKeySequence::Redo; }
void RedoAction::_execute()  { AppContext::get().undo()->redo(); }
void RedoAction::_onUpdateState(bool &enabled, bool &, bool &)
{
    enabled = AppContext::get().undo()->canRedo();
}

// ==================== CutAction ====================
QIcon CutAction::_icon() { return {}; }
QString CutAction::_text() { return QObject::tr("Cu&t"); }
QKeySequence CutAction::_shortcut() const { return QKeySequence::Cut; }
void CutAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene()) return;
    auto items = page->scene()->selectedItems();
    if (items.isEmpty()) return;

    auto *us = page->undoStack();
    us->beginMacro(QObject::tr("Cut"));
    AppContext::get().clipboard()->copy(items);
    auto deletable = ::filterSelectableItems(items);
    if (!deletable.isEmpty())
        us->push(new RemoveItemsCommand(page->scene(), deletable));
    us->endMacro();
}

// ==================== CopyAction ====================
QIcon CopyAction::_icon() { return {}; }
QString CopyAction::_text() { return QObject::tr("&Copy"); }
QKeySequence CopyAction::_shortcut() const { return QKeySequence::Copy; }
void CopyAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page) return;
    auto items = page->scene()->selectedItems();
    if (items.isEmpty()) return;
    AppContext::get().clipboard()->copy(items);
}

// ==================== PasteAction ====================
QIcon PasteAction::_icon() { return {}; }
QString PasteAction::_text() { return QObject::tr("&Paste"); }
QKeySequence PasteAction::_shortcut() const { return QKeySequence::Paste; }
void PasteAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene()) return;
    auto *clip = AppContext::get().clipboard();
    auto items = clip->paste(page->scene(), page->undoStack());
    if (items.isEmpty()) return;

    for (auto *item : items)
        item->moveBy(20, 20);
    page->undoStack()->push(new PasteItemsCommand(page->scene(), items));
    page->scene()->clearSelection();
    for (auto *item : items)
        item->setSelected(true);
}

// ==================== DeleteAction ====================
QIcon DeleteAction::_icon() { return {}; }
QString DeleteAction::_text() { return QObject::tr("&Delete"); }
QKeySequence DeleteAction::_shortcut() const { return QKeySequence::Delete; }
void DeleteAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene()) return;
    auto items = page->scene()->selectedItems();
    if (items.isEmpty()) return;
    auto deletable = ::filterSelectableItems(items);
    if (!deletable.isEmpty())
        page->undoStack()->push(new RemoveItemsCommand(page->scene(), deletable));
}

// ==================== SelectAllAction ====================
QIcon SelectAllAction::_icon() { return {}; }
QString SelectAllAction::_text() { return QObject::tr("Select &All"); }
QKeySequence SelectAllAction::_shortcut() const { return QKeySequence::SelectAll; }
void SelectAllAction::_execute()
{
    auto *page = AppContext::get().activeCanvasPage();
    if (!page || !page->scene()) return;
    auto selectable = ::filterSelectableItems(page->scene()->items());
    for (auto *item : selectable)
        item->setSelected(true);
}
