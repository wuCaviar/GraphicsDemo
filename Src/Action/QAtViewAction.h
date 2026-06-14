#ifndef QATVIEWACTION_H
#define QATVIEWACTION_H

#include "QAtActionBase.h"

class QAtGraphicsView;

// View action base — category "View", global scope
class QAtViewActionBase : public QAtActionBase
{
public:
    QAtViewActionBase() { m_category = QStringLiteral("View"); }

    QAtGraphicsView* view() const;
};

#endif // QATVIEWACTION_H
