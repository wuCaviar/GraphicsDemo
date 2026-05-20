#include "QATActionBase.h"

#include <QCoreApplication>

// ============================================================================
// QhsActionFactory
// ============================================================================

QhsActionFactory::QhsActionFactory()
{
}

QhsActionFactory& QhsActionFactory::get()
{
    static QhsActionFactory instance;
    return instance;
}

void QhsActionFactory::_registerAction(const QString& token, ActionCreateFunc func)
{
    m_mapCreateFunc.insert(token, func);
}

QhsActionBasePtr QhsActionFactory::createAction(const QString& token)
{
    auto it = m_mapCreateFunc.find(token);
    if (it != m_mapCreateFunc.end())
        return it.value()();
    return {};
}

// ============================================================================
// QhsActionFactory::_Register
// ============================================================================

QhsActionFactory::_Register::_Register(const QString& token, ActionCreateFunc func)
{
    QhsActionFactory::get()._registerAction(token, func);
}

// ============================================================================
// QhsActionBase
// ============================================================================

void QhsActionBase::onUpdateState(bool& isEnabled, bool& isChecked, bool& isVisible)
{
    _onUpdateState(isEnabled, isChecked, isVisible);
}

void QhsActionBase::_onUpdateState(bool& isEnabled, bool& isChecked, bool& isVisible)
{
    Q_UNUSED(isEnabled);
    Q_UNUSED(isChecked);
    Q_UNUSED(isVisible);
}
