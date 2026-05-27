#include "QAtActionBase.h"

#include <QCoreApplication>

// ============================================================================
// QAtActionFactory
// ============================================================================

QAtActionFactory::QAtActionFactory()
{
}

QAtActionFactory& QAtActionFactory::get()
{
    static QAtActionFactory instance;
    return instance;
}

void QAtActionFactory::_registerAction(const QString& token, ActionCreateFunc func)
{
    m_mapCreateFunc.insert(token, func);
}

QAtActionBasePtr QAtActionFactory::createAction(const QString& token)
{
    auto it = m_mapCreateFunc.find(token);
    if (it != m_mapCreateFunc.end())
        return it.value()();
    return {};
}

// ============================================================================
// QAtActionFactory::_Register
// ============================================================================

QAtActionFactory::_Register::_Register(const QString& token, ActionCreateFunc func)
{
    QAtActionFactory::get()._registerAction(token, func);
}

// ============================================================================
// QAtActionBase
// ============================================================================

void QAtActionBase::onUpdateState(bool& isEnabled, bool& isChecked, bool& isVisible)
{
    _onUpdateState(isEnabled, isChecked, isVisible);
}

void QAtActionBase::_onUpdateState(bool& isEnabled, bool& isChecked, bool& isVisible)
{
    Q_UNUSED(isEnabled);
    Q_UNUSED(isChecked);
    Q_UNUSED(isVisible);
}
