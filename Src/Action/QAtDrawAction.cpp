#include "QAtDrawAction.h"

#include "qatgraphicsview.h"

QAtDrawActionBase::QAtDrawActionBase() : QAtActionBase() { }

void QAtDrawActionBase::setAttchViewScene(QGraphicsView *view,
                                          QGraphicsScene *scene)
{
    m_pView = view;
    m_pScene = scene;
}

void QAtDrawActionBase::_onUpdateState(bool &isEnabled, bool &isChecked,
                                       bool &isVisible)
{
    // 没有画布时不允许使用
    bool result = (m_pView != nullptr && m_pScene != nullptr);
    if (result) {
        isEnabled =
            static_cast<QAtGraphicsView *>(m_pView)->canvasItem() != nullptr;
    }
    isChecked = true;
    isVisible = true;
}

void QAtDrawActionBase::_execute() { }

///////////////////////////////////////////////////////////////////////////////
QAtDrawRectAction::QAtDrawRectAction() : QAtDrawActionBase() { }

void QAtDrawRectAction::_onUpdateState(bool &isEnabled, bool &isChecked,
                                       bool &isVisible)
{
    QAtDrawActionBase::_onUpdateState(isEnabled, isChecked, isVisible);
}

void QAtDrawRectAction::_execute() { }
