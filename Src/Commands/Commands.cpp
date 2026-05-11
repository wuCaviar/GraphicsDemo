#include "Commands.h"
#include "RectItem.h"

#include <QGraphicsScene>

// ============================================================
// AddItemCommand
// ============================================================
AddItemCommand::AddItemCommand(QGraphicsScene *scene, QGraphicsItem *item, QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene), m_item(item)
{
    setText(QObject::tr("Add Item"));
}

AddItemCommand::~AddItemCommand()
{
    // 场景已销毁时，item 也必然已被 Qt 对象树销毁，无需再 delete
    if (!m_scene)
        return;
    // 仅当 command 仍持有 item 所有权且 item 不在场景中时才删除
    if (m_owned && m_item && !m_item->scene())
        delete m_item;
}

void AddItemCommand::undo()
{
    if (!m_scene || !m_item)
        return;
    m_scene->removeItem(m_item);
    m_owned = true;
}

void AddItemCommand::redo()
{
    if (!m_scene || !m_item)
        return;
    m_scene->addItem(m_item);
    m_owned = false;
}

// ============================================================
// RemoveItemsCommand
// ============================================================
RemoveItemsCommand::RemoveItemsCommand(QGraphicsScene *scene,
                                       const QList<QGraphicsItem *> &items,
                                       QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene), m_items(items)
{
    setText(QObject::tr("Delete Items"));
}

RemoveItemsCommand::~RemoveItemsCommand()
{
    // 场景已销毁时，items 也必然已被销毁
    if (!m_scene)
        return;
    // 仅当 command 仍持有 items 所有权（redo 状态，item 已从场景移除）时才删除
    if (m_owned) {
        for (auto *item : m_items) {
            if (item && !item->scene())
                delete item;
        }
    }
}

void RemoveItemsCommand::undo()
{
    if (!m_scene)
        return;
    for (auto *item : m_items) {
        if (item)
            m_scene->addItem(item);
    }
    m_owned = false; // items 回到场景，command 不再持有
}

void RemoveItemsCommand::redo()
{
    if (!m_scene)
        return;
    for (auto *item : m_items) {
        if (item)
            m_scene->removeItem(item);
    }
    m_owned = true; // items 从场景移除，command 持有
}

// ============================================================
// MoveItemsCommand
// ============================================================
MoveItemsCommand::MoveItemsCommand(const QList<QGraphicsItem *> &items,
                                   const QList<QPointF> &oldPositions,
                                   const QList<QPointF> &newPositions,
                                   QGraphicsScene *scene,
                                   QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_items(items)
    , m_oldPos(oldPositions)
    , m_newPos(newPositions)
    , m_scene(scene)
{
    setText(QObject::tr("Move Items"));
}

void MoveItemsCommand::undo()
{
    // 场景已销毁，所有图元已无效
    if (!m_scene)
        return;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i])
            m_items[i]->setPos(m_oldPos[i]);
    }
}

void MoveItemsCommand::redo()
{
    // 场景已销毁，所有图元已无效
    if (!m_scene)
        return;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i])
            m_items[i]->setPos(m_newPos[i]);
    }
}

// ============================================================
// PropertyChangeCommand
// ============================================================
PropertyChangeCommand::PropertyChangeCommand(QGraphicsItem *item, PropType propType,
                                             const QVariant &oldValue, const QVariant &newValue,
                                             QGraphicsScene *scene,
                                             QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_item(item)
    , m_propType(propType)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
    , m_scene(scene)
{
    switch (propType) {
    case Pen:          setText(QObject::tr("Change Pen")); break;
    case Brush:        setText(QObject::tr("Change Brush")); break;
    case Font:         setText(QObject::tr("Change Font")); break;
    case Text:         setText(QObject::tr("Change Text")); break;
    case Geometry:     setText(QObject::tr("Resize")); break;
    case CornerRadius: setText(QObject::tr("Change Corner Radius")); break;
    }
}

void PropertyChangeCommand::undo()
{
    // 场景已销毁，图元必然已无效
    if (!m_scene)
        return;
    if (!m_item) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_item);
    if (!gi) return;

    switch (m_propType) {
    case Pen:          gi->setItemPen(m_oldValue.value<QPen>()); break;
    case Brush:        gi->setItemBrush(m_oldValue.value<QBrush>()); break;
    case Font:         gi->setItemFont(m_oldValue.value<QFont>()); break;
    case Text:         gi->setText(m_oldValue.toString()); break;
    case Geometry:
        gi->setGeometryRect(m_oldValue.toRectF());
        break;
    case CornerRadius:
        if (auto *ri = qgraphicsitem_cast<RectItem *>(m_item))
            ri->setCornerRadius(m_oldValue.toReal());
        break;
    }
}

void PropertyChangeCommand::redo()
{
    // 场景已销毁，图元必然已无效
    if (!m_scene)
        return;
    if (!m_item) return;
    auto *gi = dynamic_cast<IGraphicsItem *>(m_item);
    if (!gi) return;

    switch (m_propType) {
    case Pen:          gi->setItemPen(m_newValue.value<QPen>()); break;
    case Brush:        gi->setItemBrush(m_newValue.value<QBrush>()); break;
    case Font:         gi->setItemFont(m_newValue.value<QFont>()); break;
    case Text:         gi->setText(m_newValue.toString()); break;
    case Geometry:
        gi->setGeometryRect(m_newValue.toRectF());
        break;
    case CornerRadius:
        if (auto *ri = qgraphicsitem_cast<RectItem *>(m_item))
            ri->setCornerRadius(m_newValue.toReal());
        break;
    }
}

// ============================================================
// PasteItemsCommand
// ============================================================
PasteItemsCommand::PasteItemsCommand(QGraphicsScene *scene,
                                     const QList<QGraphicsItem *> &items,
                                     QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene), m_items(items)
{
    setText(QObject::tr("Paste Items"));
}

PasteItemsCommand::~PasteItemsCommand()
{
    // 场景已销毁时，items 也必然已被销毁
    if (!m_scene)
        return;
    // 仅当 command 仍持有 items 所有权（undo 状态，item 已从场景移除）时才删除
    if (m_owned) {
        for (auto *item : m_items) {
            if (item && !item->scene())
                delete item;
        }
    }
}

void PasteItemsCommand::undo()
{
    if (!m_scene)
        return;
    for (auto *item : m_items) {
        if (item)
            m_scene->removeItem(item);
    }
    m_owned = true; // items 从场景移除，command 持有
}

void PasteItemsCommand::redo()
{
    if (!m_scene)
        return;
    for (auto *item : m_items) {
        if (item)
            m_scene->addItem(item);
    }
    m_owned = false; // items 添加到场景，command 不再持有
}

// ============================================================
// ZValueChangeCommand
// ============================================================
ZValueChangeCommand::ZValueChangeCommand(const QList<QGraphicsItem *> &items,
                                         const QList<qreal> &oldZ,
                                         const QList<qreal> &newZ,
                                         QGraphicsScene *scene,
                                         QUndoCommand *parent)
    : QUndoCommand(parent), m_items(items), m_oldZ(oldZ), m_newZ(newZ), m_scene(scene)
{
    setText(QObject::tr("Change Z-Order"));
}

void ZValueChangeCommand::undo()
{
    if (!m_scene)
        return;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i])
            m_items[i]->setZValue(m_oldZ[i]);
    }
}

void ZValueChangeCommand::redo()
{
    if (!m_scene)
        return;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i])
            m_items[i]->setZValue(m_newZ[i]);
    }
}

// ============================================================
// AlignItemsCommand
// ============================================================
AlignItemsCommand::AlignItemsCommand(const QList<QGraphicsItem *> &items,
                                     const QList<QPointF> &oldPositions,
                                     const QList<QPointF> &newPositions,
                                     const QString &description,
                                     QGraphicsScene *scene,
                                     QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_items(items)
    , m_oldPos(oldPositions)
    , m_newPos(newPositions)
    , m_scene(scene)
{
    setText(description.isEmpty() ? QObject::tr("Align Items") : description);
}

void AlignItemsCommand::undo()
{
    if (!m_scene)
        return;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i])
            m_items[i]->setPos(m_oldPos[i]);
    }
}

void AlignItemsCommand::redo()
{
    if (!m_scene)
        return;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i])
            m_items[i]->setPos(m_newPos[i]);
    }
}

// ============================================================
// PositionChangeCommand（单图元位置变更，由属性面板触发）
// ============================================================
PositionChangeCommand::PositionChangeCommand(QGraphicsItem *item, const QPointF &oldPos,
                                             const QPointF &newPos, QGraphicsScene *scene,
                                             QUndoCommand *parent)
    : QUndoCommand(parent), m_item(item), m_oldPos(oldPos), m_newPos(newPos), m_scene(scene)
{
    setText(QObject::tr("Change Position"));
}

void PositionChangeCommand::undo()
{
    if (!m_scene || !m_item)
        return;
    m_item->setPos(m_oldPos);
}

void PositionChangeCommand::redo()
{
    if (!m_scene || !m_item)
        return;
    m_item->setPos(m_newPos);
}

// ============================================================
// RotationChangeCommand（单图元绕中心旋转变更）
// ============================================================
RotationChangeCommand::RotationChangeCommand(QGraphicsItem *item, qreal oldRotation,
                                             qreal newRotation, QGraphicsScene *scene,
                                             QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_item(item)
    , m_oldRotation(oldRotation)
    , m_newRotation(newRotation)
    , m_oldPos(item->pos())
    , m_scene(scene)
{
    // 纯数学计算中心旋转的位置补偿（不修改 item 状态）
    // 旋转绕 transformOriginPoint 进行，需要补偿使 boundingRect 中心保持不变
    QPointF origin = item->mapToScene(item->transformOriginPoint());
    QPointF oldCenter = item->mapToScene(item->boundingRect().center());

    // 用 QTransform 计算旋转后中心位置
    qreal delta = newRotation - oldRotation;
    QTransform t;
    t.translate(origin.x(), origin.y());
    t.rotate(delta);
    t.translate(-origin.x(), -origin.y());
    QPointF newCenter = t.map(oldCenter);

    // 补偿位置使中心保持不变
    m_newPos = m_oldPos + (oldCenter - newCenter);

    setText(QObject::tr("Rotate %1°").arg(delta, 0, 'f', 1));
}

void RotationChangeCommand::undo()
{
    if (!m_scene || !m_item)
        return;
    m_item->setRotation(m_oldRotation);
    m_item->setPos(m_oldPos);
}

void RotationChangeCommand::redo()
{
    if (!m_scene || !m_item)
        return;
    m_item->setRotation(m_newRotation);
    m_item->setPos(m_newPos);
}
