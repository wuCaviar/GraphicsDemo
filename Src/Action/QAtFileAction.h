#ifndef QATFILEACTION_H
#define QATFILEACTION_H

#include "QAtActionBase.h"

// File action base — category "File", global scope (pageTypes empty)
class QAtFileActionBase : public QAtActionBase
{
public:
    QAtFileActionBase() { m_category = QStringLiteral("File"); }
};

#endif // QATFILEACTION_H
