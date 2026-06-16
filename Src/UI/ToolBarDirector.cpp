#include "ToolBarDirector.h"
#include "QtitanDocking.h"

ToolBarDirector::ToolBarDirector(QObject *parent)
    : QObject(parent)
{
}

void ToolBarDirector::addToolBar(DockToolBar *bar, const QSet<QString> &pageTypes)
{
    if (!bar) return;
    m_visibilityRules[bar] = pageTypes;
}

void ToolBarDirector::applyVisibility(const QString &pageType)
{
    for (auto it = m_visibilityRules.begin(); it != m_visibilityRules.end(); ++it) {
        QSet<QString> types = it.value();
        it.key()->setVisible(types.isEmpty() || types.contains(pageType));
    }
}

void ToolBarDirector::onPageSwitched(const QString &, const QString &pageType)
{
    applyVisibility(pageType);
}
