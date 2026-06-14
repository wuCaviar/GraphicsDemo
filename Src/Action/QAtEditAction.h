#ifndef QATEDITACTION_H
#define QATEDITACTION_H

#include "QAtActionBase.h"

#include <QGraphicsScene>
#include <QUndoStack>

class QAtGraphicsView;

// Edit action base — category "Edit", canvas-only
class QAtEditActionBase : public QAtActionBase
{
public:
    QAtEditActionBase()
    {
        m_category = QStringLiteral("Edit");
        m_pageTypes = { QStringLiteral("canvas") };
    }

    QGraphicsScene* scene() const;
    QUndoStack*     undoStack() const;
    QAtGraphicsView* view() const;
};

#endif // QATEDITACTION_H
