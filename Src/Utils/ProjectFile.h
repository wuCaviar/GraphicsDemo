#ifndef PROJECTFILE_H
#define PROJECTFILE_H

#include "IGraphicsItem.h"

#include <QDomDocument>
#include <QGraphicsItem>
#include <QString>
#include <QList>

class ProjectFile
{
public:
    struct ProjectInfo {
        QString name;
        QString version = QStringLiteral("1.0.0");
        QString author;
        QString description;
    };

    struct CanvasInfo {
        double width = 1920.0;
        double height = 1080.0;
        double dpi = 96.0;
    };

    bool save(const QString &filePath, const ProjectInfo &info,
              const CanvasInfo &canvas, const QList<QGraphicsItem *> &items);

    // 返回新创建的图元列表（调用者负责管理生命周期）
    bool load(const QString &filePath, ProjectInfo &info, CanvasInfo &canvas,
              QList<QGraphicsItem *> &items);

    QString lastError() const { return m_lastError; }

private:
    QByteArray encrypt(const QByteArray &data, const QByteArray &key);
    QByteArray decrypt(const QByteArray &data, const QByteArray &key);

    QDomElement serializeItem(QDomDocument &doc, QGraphicsItem *item, int id);
    IGraphicsItem *deserializeItem(const QDomElement &el);

    QString m_lastError;
};

#endif // PROJECTFILE_H
