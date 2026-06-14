#ifndef ARRANGEACTIONS_H
#define ARRANGEACTIONS_H

#include "QAtArrangeAction.h"

class BringToFrontAction : public QAtArrangeActionBase
{
public:
    BringToFrontAction() { m_strActionToken = QStringLiteral("BringToFront"); }
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class SendToBackAction : public QAtArrangeActionBase
{
public:
    SendToBackAction() { m_strActionToken = QStringLiteral("SendToBack"); }
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class GroupAction : public QAtArrangeActionBase
{
public:
    GroupAction() { m_strActionToken = QStringLiteral("Group"); }
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class UngroupAction : public QAtArrangeActionBase
{
public:
    UngroupAction() { m_strActionToken = QStringLiteral("Ungroup"); }
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class FitCanvasToItemsAction : public QAtArrangeActionBase
{
public:
    FitCanvasToItemsAction() { m_strActionToken = QStringLiteral("FitCanvasToItems"); }
    QString _text() override;
    void _execute() override;
};

class RotateCWAction : public QAtArrangeActionBase
{
public:
    RotateCWAction() { m_strActionToken = QStringLiteral("RotateCW"); }
    QIcon _icon() override;
    QString _text() override;
    void _execute() override;
};

class RotateCCWAction : public QAtArrangeActionBase
{
public:
    RotateCCWAction() { m_strActionToken = QStringLiteral("RotateCCW"); }
    QIcon _icon() override;
    QString _text() override;
    void _execute() override;
};

class Rotate180Action : public QAtArrangeActionBase
{
public:
    Rotate180Action() { m_strActionToken = QStringLiteral("Rotate180"); }
    QString _text() override;
    void _execute() override;
};

class AutoLayoutAction : public QAtArrangeActionBase
{
public:
    AutoLayoutAction() { m_strActionToken = QStringLiteral("AutoLayout"); }
    QIcon _icon() override;
    QString _text() override;
    void _execute() override;
};

#endif // ARRANGEACTIONS_H
