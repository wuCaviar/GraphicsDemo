#ifndef QATDRAWACTION_H
#define QATDRAWACTION_H

#include "QAtActionBase.h"

enum class Tool;

class QAtGraphicsView;

// Drawing tool base — category "Draw", canvas-only, checkable
class QAtDrawActionBase : public QAtActionBase
{
public:
    QAtDrawActionBase();

    virtual Tool _tool() const = 0;
    void switchToTool();

    QAtGraphicsView* view() const;

protected:
    void _onUpdateState(bool &isEnabled, bool &isChecked, bool &isVisible) override;
    void _execute() override;
};

// Concrete tools
class SelectToolAction : public QAtDrawActionBase
{
public:
    SelectToolAction() { m_strActionToken = QStringLiteral("SelectTool"); }
    QIcon _icon() override;
    QString _text() override;
    Tool _tool() const override;
};

class HandToolAction : public QAtDrawActionBase
{
public:
    HandToolAction() { m_strActionToken = QStringLiteral("HandTool"); }
    QIcon _icon() override;
    QString _text() override;
    Tool _tool() const override;
};

class RectToolAction : public QAtDrawActionBase
{
public:
    RectToolAction() { m_strActionToken = QStringLiteral("RectTool"); }
    QIcon _icon() override;
    QString _text() override;
    Tool _tool() const override;
};

class EllipseToolAction : public QAtDrawActionBase
{
public:
    EllipseToolAction() { m_strActionToken = QStringLiteral("EllipseTool"); }
    QIcon _icon() override;
    QString _text() override;
    Tool _tool() const override;
};

class LineToolAction : public QAtDrawActionBase
{
public:
    LineToolAction() { m_strActionToken = QStringLiteral("LineTool"); }
    QIcon _icon() override;
    QString _text() override;
    Tool _tool() const override;
};

class BezierCurveToolAction : public QAtDrawActionBase
{
public:
    BezierCurveToolAction() { m_strActionToken = QStringLiteral("BezierCurveTool"); }
    QIcon _icon() override;
    QString _text() override;
    Tool _tool() const override;
};

class FreehandToolAction : public QAtDrawActionBase
{
public:
    FreehandToolAction() { m_strActionToken = QStringLiteral("FreehandTool"); }
    QIcon _icon() override;
    QString _text() override;
    Tool _tool() const override;
};

class TextToolAction : public QAtDrawActionBase
{
public:
    TextToolAction() { m_strActionToken = QStringLiteral("TextTool"); }
    QIcon _icon() override;
    QString _text() override;
    Tool _tool() const override;
};

#endif // QATDRAWACTION_H
