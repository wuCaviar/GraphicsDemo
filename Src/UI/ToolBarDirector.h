#ifndef TOOLBARDIRECTOR_H
#define TOOLBARDIRECTOR_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>

#include "QtitanDocking.h"

// Manages toolbar visibility based on active Page type
// Toolbars are created externally and registered here for visibility rules
class ToolBarDirector : public QObject
{
    Q_OBJECT
public:
    explicit ToolBarDirector(QObject *parent = nullptr);

    // Register an externally-created toolbar with page-type visibility rules.
    // Empty pageTypes = always visible.
    void addToolBar(DockToolBar *bar, const QSet<QString> &pageTypes = {});

    // Apply visibility rules immediately (call after registering all toolbars)
    void applyVisibility(const QString &pageType);

public slots:
    void onPageSwitched(const QString &pageId, const QString &pageType);

private:
    QMap<DockToolBar*, QSet<QString>> m_visibilityRules;
};

#endif // TOOLBARDIRECTOR_H
