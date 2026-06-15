#include "QAtActionBase.h"

#include <QCoreApplication>

// ============================================================================
// QAtActionFactory
// ============================================================================

QAtActionFactory::QAtActionFactory() { }

QAtActionFactory &QAtActionFactory::get()
{
    static QAtActionFactory instance;
    return instance;
}

void QAtActionFactory::_registerAction(const QString &token, ActionCreateFunc func)
{
    m_mapCreateFunc.insert(token, func);
}

QAtActionBasePtr QAtActionFactory::createAction(const QString &token)
{
    auto it = m_mapCreateFunc.find(token);
    if (it != m_mapCreateFunc.end())
        return it.value()();
    return { };
}

// ============================================================================
// QAtActionFactory::_Register
// ============================================================================

QAtActionFactory::_Register::_Register(const QString &token, ActionCreateFunc func)
{
    QAtActionFactory::get()._registerAction(token, func);
}

// ============================================================================
// QAtActionBase
// ============================================================================

void QAtActionBase::onUpdateState(bool &isEnabled, bool &isChecked, bool &isVisible)
{
    _onUpdateState(isEnabled, isChecked, isVisible);
}

void QAtActionBase::_onUpdateState(bool &isEnabled, bool &isChecked, bool &isVisible)
{
    Q_UNUSED(isEnabled);
    Q_UNUSED(isChecked);
    Q_UNUSED(isVisible);
}

QAction *QAtActionBase::createQAction(QObject *parent)
{
    QAction *act = new QAction(parent);
    act->setIcon(_icon());
    act->setText(_text());
    act->setToolTip(_tooltip());
    act->setShortcut(_shortcut());
    act->setCheckable(_isCheckable());
    // Wire QAction::triggered → QAtActionBase::_execute()
    QObject::connect(act, &QAction::triggered, act, [this]() { this->_execute(); });
    return act;
}

void QAtActionBase::updateQAction(QAction *act)
{
    if (!act)
        return;
    bool enabled = true, checked = false, visible = true;
    _onUpdateState(enabled, checked, visible);
    act->setEnabled(enabled);
    act->setChecked(checked);
    act->setVisible(visible);
}
