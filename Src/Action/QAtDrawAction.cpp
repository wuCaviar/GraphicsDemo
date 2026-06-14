#include "QAtDrawAction.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"
#include "qatgraphicsview.h"

#include <QGraphicsView>

QAtDrawActionBase::QAtDrawActionBase()
{
    m_category = QStringLiteral("Draw");
    m_pageTypes = { QStringLiteral("canvas") };
    m_checkable = true;
}

void QAtDrawActionBase::switchToTool()
{
    auto *v = view();
    if (v) v->setTool(_tool());
}

void QAtDrawActionBase::_execute()
{
    switchToTool();
}

QAtGraphicsView* QAtDrawActionBase::view() const
{
    auto *p = AppContext::get().activeCanvasPage();
    return p ? p->view() : nullptr;
}

void QAtDrawActionBase::_onUpdateState(bool &isEnabled, bool &isChecked, bool &isVisible)
{
    auto *v = view();
    isEnabled = v && v->canvasItem() != nullptr;
    isChecked = v && v->currentTool() == _tool();
    isVisible = true;
}

// ==================== Concrete tools ====================
Tool SelectToolAction::_tool() const       { return Tool::Select; }
Tool HandToolAction::_tool() const         { return Tool::Hand; }
Tool RectToolAction::_tool() const         { return Tool::Rect; }
Tool EllipseToolAction::_tool() const      { return Tool::Ellipse; }
Tool LineToolAction::_tool() const         { return Tool::Line; }
Tool BezierCurveToolAction::_tool() const  { return Tool::BezierCurve; }
Tool FreehandToolAction::_tool() const     { return Tool::Freehand; }
Tool TextToolAction::_tool() const         { return Tool::Text; }

QIcon SelectToolAction::_icon()       { return QIcon(QStringLiteral(":/icons/icons/tool-select.svg")); }
QIcon HandToolAction::_icon()          { return QIcon(); }
QIcon RectToolAction::_icon()          { return QIcon(QStringLiteral(":/icons/icons/tool-rect.svg")); }
QIcon EllipseToolAction::_icon()       { return QIcon(QStringLiteral(":/icons/icons/tool-ellipse.svg")); }
QIcon LineToolAction::_icon()          { return QIcon(QStringLiteral(":/icons/icons/tool-line.svg")); }
QIcon BezierCurveToolAction::_icon()   { return QIcon(QStringLiteral(":/icons/icons/tool-curve.svg")); }
QIcon FreehandToolAction::_icon()      { return QIcon(QStringLiteral(":/icons/icons/tool-freehand.svg")); }
QIcon TextToolAction::_icon()          { return QIcon(QStringLiteral(":/icons/icons/tool-text.svg")); }

QString SelectToolAction::_text()       { return QObject::tr("Select"); }
QString HandToolAction::_text()         { return QObject::tr("Hand"); }
QString RectToolAction::_text()         { return QObject::tr("Rectangle"); }
QString EllipseToolAction::_text()      { return QObject::tr("Ellipse"); }
QString LineToolAction::_text()         { return QObject::tr("Line"); }
QString BezierCurveToolAction::_text()  { return QObject::tr("Bezier Curve"); }
QString FreehandToolAction::_text()     { return QObject::tr("Freehand"); }
QString TextToolAction::_text()         { return QObject::tr("Text"); }
