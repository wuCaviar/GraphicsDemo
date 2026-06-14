#ifndef EDITACTIONS_H
#define EDITACTIONS_H

#include "QAtEditAction.h"

class UndoAction : public QAtEditActionBase
{
public:
    UndoAction() { m_strActionToken = QStringLiteral("Undo"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
    void _onUpdateState(bool &enabled, bool &checked, bool &visible) override;
};

class RedoAction : public QAtEditActionBase
{
public:
    RedoAction() { m_strActionToken = QStringLiteral("Redo"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
    void _onUpdateState(bool &enabled, bool &checked, bool &visible) override;
};

class CutAction : public QAtEditActionBase
{
public:
    CutAction() { m_strActionToken = QStringLiteral("Cut"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class CopyAction : public QAtEditActionBase
{
public:
    CopyAction() { m_strActionToken = QStringLiteral("Copy"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class PasteAction : public QAtEditActionBase
{
public:
    PasteAction() { m_strActionToken = QStringLiteral("Paste"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class DeleteAction : public QAtEditActionBase
{
public:
    DeleteAction() { m_strActionToken = QStringLiteral("Delete"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class SelectAllAction : public QAtEditActionBase
{
public:
    SelectAllAction() { m_strActionToken = QStringLiteral("SelectAll"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

#endif // EDITACTIONS_H
