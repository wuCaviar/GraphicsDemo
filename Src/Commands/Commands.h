#ifndef COMMANDS_H
#define COMMANDS_H

#include "IGraphicsItem.h"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPointer>
#include <QUndoCommand>

class QGraphicsScene;

// ============================================================
// 添加图元
// ============================================================
class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(QGraphicsScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr);
    ~AddItemCommand();

    void undo() override;
    void redo() override;

private:
    QPointer<QGraphicsScene> m_scene;
    QGraphicsItem *m_item;
    bool m_owned = true; // 首次 redo 前自己持有指针
};

// ============================================================
// 删除图元
// ============================================================
class RemoveItemsCommand : public QUndoCommand
{
public:
    RemoveItemsCommand(QGraphicsScene *scene, const QList<QGraphicsItem *> &items,
                       QUndoCommand *parent = nullptr);
    ~RemoveItemsCommand();

    void undo() override;
    void redo() override;

private:
    QPointer<QGraphicsScene> m_scene;
    QList<QGraphicsItem *> m_items;
    bool m_owned = false; // redo 后（item 从场景移除）为 true
};

// ============================================================
// 移动图元
// ============================================================
class MoveItemsCommand : public QUndoCommand
{
public:
    MoveItemsCommand(const QList<QGraphicsItem *> &items,
                     const QList<QPointF> &oldPositions,
                     const QList<QPointF> &newPositions,
                     QGraphicsScene *scene = nullptr,
                     QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    QList<QGraphicsItem *> m_items;
    QList<QPointF> m_oldPos;
    QList<QPointF> m_newPos;
    QPointer<QGraphicsScene> m_scene; // 用于检测场景是否已被销毁
};

// ============================================================
// 属性变更（通用）
// ============================================================
class PropertyChangeCommand : public QUndoCommand
{
public:
    enum PropType { Pen, Brush, Font, Text, Geometry, CornerRadius };

    PropertyChangeCommand(QGraphicsItem *item, PropType propType,
                          const QVariant &oldValue, const QVariant &newValue,
                          QGraphicsScene *scene = nullptr,
                          QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    QGraphicsItem *m_item;
    PropType m_propType;
    QVariant m_oldValue;
    QVariant m_newValue;
    QPointer<QGraphicsScene> m_scene; // 用于检测场景是否已被销毁
};

// ============================================================
// 粘贴图元
// ============================================================
class PasteItemsCommand : public QUndoCommand
{
public:
    PasteItemsCommand(QGraphicsScene *scene, const QList<QGraphicsItem *> &items,
                      QUndoCommand *parent = nullptr);
    ~PasteItemsCommand();

    void undo() override;
    void redo() override;

private:
    QPointer<QGraphicsScene> m_scene;
    QList<QGraphicsItem *> m_items;
    bool m_owned = false; // undo 后（item 从场景移除）为 true
};

// ============================================================
// Z 值变更（置顶/置底）
// ============================================================
class ZValueChangeCommand : public QUndoCommand
{
public:
    ZValueChangeCommand(const QList<QGraphicsItem *> &items,
                        const QList<qreal> &oldZ, const QList<qreal> &newZ,
                        QGraphicsScene *scene = nullptr,
                        QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    QList<QGraphicsItem *> m_items;
    QList<qreal> m_oldZ;
    QList<qreal> m_newZ;
    QPointer<QGraphicsScene> m_scene; // 用于检测场景是否已被销毁
};

// ============================================================
// 对齐/分布（位置批量变更）
// ============================================================
class AlignItemsCommand : public QUndoCommand
{
public:
    AlignItemsCommand(const QList<QGraphicsItem *> &items,
                      const QList<QPointF> &oldPositions,
                      const QList<QPointF> &newPositions,
                      const QString &description = QString(),
                      QGraphicsScene *scene = nullptr,
                      QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    QList<QGraphicsItem *> m_items;
    QList<QPointF> m_oldPos;
    QList<QPointF> m_newPos;
    QPointer<QGraphicsScene> m_scene; // 用于检测场景是否已被销毁
};

// ============================================================
// 位置变更（单个图元，由属性面板触发）
// ============================================================
class PositionChangeCommand : public QUndoCommand
{
public:
    PositionChangeCommand(QGraphicsItem *item, const QPointF &oldPos, const QPointF &newPos,
                          QGraphicsScene *scene = nullptr, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    QGraphicsItem *m_item;
    QPointF m_oldPos;
    QPointF m_newPos;
    QPointer<QGraphicsScene> m_scene; // 用于检测场景是否已被销毁
};

// ============================================================
// 旋转变更（单个图元，由属性面板或旋转按钮触发）
// 绕图元中心旋转：通过位置补偿确保旋转中心为 boundingRect().center()
// ============================================================
class RotationChangeCommand : public QUndoCommand
{
public:
    RotationChangeCommand(QGraphicsItem *item, qreal oldRotation, qreal newRotation,
                          QGraphicsScene *scene = nullptr, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    QGraphicsItem *m_item;
    qreal m_oldRotation;
    qreal m_newRotation;
    QPointF m_oldPos;  // 旋转前位置
    QPointF m_newPos;  // 中心旋转补偿后的位置
    QPointer<QGraphicsScene> m_scene;
};

#endif // COMMANDS_H
