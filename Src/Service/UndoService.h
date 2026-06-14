#ifndef UNDOSERVICE_H
#define UNDOSERVICE_H

#include "QAtService.h"

class QUndoStack;

// 活动画布的 undo/redo 代理 — 监听 pageSwitched 自动绑定 undoStack
class UndoService : public QAtService
{
    Q_OBJECT
public:
    explicit UndoService(QObject *parent = nullptr);

    QString serviceId() const override { return QStringLiteral("undo"); }
    QString description() const override;

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    void bindUndoStack(QUndoStack *stack);

signals:
    void undoStateChanged(bool canUndo, bool canRedo);

private:
    QUndoStack *m_stack = nullptr;
};

#endif // UNDOSERVICE_H
