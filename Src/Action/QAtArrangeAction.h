#ifndef QATARRANGEACTION_H
#define QATARRANGEACTION_H

#include "QAtActionBase.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QUndoStack>

class QAtGraphicsView;

// Arrange action base — category "Arrange", canvas-only
class QAtArrangeActionBase : public QAtActionBase
{
public:
    QAtArrangeActionBase()
    {
        m_category = QStringLiteral("Arrange");
        m_pageTypes = { QStringLiteral("canvas") };
    }

    QGraphicsScene* scene() const;
    QUndoStack*         undoStack() const;
    QAtGraphicsView*     view() const;
    QList<QGraphicsItem*> selectedItems() const;
};

#endif // QATARRANGEACTION_H
