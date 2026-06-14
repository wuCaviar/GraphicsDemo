#include "UndoService.h"

#include <QUndoStack>

UndoService::UndoService(QObject *parent) : QAtService(parent) {}

QString UndoService::description() const { return tr("Undo/Redo proxy service"); }

void UndoService::bindUndoStack(QUndoStack *stack)
{
    if (m_stack) {
        disconnect(m_stack, &QUndoStack::canUndoChanged, this, nullptr);
        disconnect(m_stack, &QUndoStack::canRedoChanged, this, nullptr);
    }
    m_stack = stack;
    if (m_stack) {
        connect(m_stack, &QUndoStack::canUndoChanged, this,
                [this](bool can) { emit undoStateChanged(can, canRedo()); });
        connect(m_stack, &QUndoStack::canRedoChanged, this,
                [this](bool can) { emit undoStateChanged(canUndo(), can); });
        // Emit initial state
        emit undoStateChanged(m_stack->canUndo(), m_stack->canRedo());
    }
}

void UndoService::undo()       { if (m_stack) m_stack->undo(); }
void UndoService::redo()       { if (m_stack) m_stack->redo(); }
bool UndoService::canUndo() const { return m_stack ? m_stack->canUndo() : false; }
bool UndoService::canRedo() const { return m_stack ? m_stack->canRedo() : false; }
