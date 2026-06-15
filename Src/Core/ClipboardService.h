#ifndef CLIPBOARDSERVICE_H
#define CLIPBOARDSERVICE_H

#include "QAtService.h"

#include <QList>
#include <QString>

class QGraphicsItem;
class QGraphicsScene;
class QMimeData;

// 剪贴板服务 — 图元序列化/反序列化、copy/paste/cut 操作
class ClipboardService : public QAtService
{
    Q_OBJECT

public:
    explicit ClipboardService(QObject *parent = nullptr);

    QString serviceId() const override { return QStringLiteral("clipboard"); }
    QString description() const override;

    void copy(const QList<QGraphicsItem *> &items);
    void cut(const QList<QGraphicsItem *> &items, QGraphicsScene *scene,
             class QUndoStack *undoStack);
    QList<QGraphicsItem *> paste(QGraphicsScene *scene, class QUndoStack *undoStack);

    static QString mimeType();

private:
    struct SerializedItem
    {
        quint32 len;
        QByteArray data;
    };
};

#endif // CLIPBOARDSERVICE_H
