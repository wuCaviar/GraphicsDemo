#ifndef QATDIALOGACTION_H
#define QATDIALOGACTION_H

#include "QAtActionBase.h"

// Dialog action base — category "Settings", global scope
class QAtDialogActionBase : public QAtActionBase
{
public:
    QAtDialogActionBase() { m_category = QStringLiteral("Settings"); }
};

#endif // QATDIALOGACTION_H
