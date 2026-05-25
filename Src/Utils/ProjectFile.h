#ifndef PROJECTFILE_H
#define PROJECTFILE_H

#include "IGraphicsItem.h"

#include <QByteArray>
#include <QDomDocument>
#include <QGraphicsItem>
#include <QList>
#include <QString>

// ---- 并发序列化/反序列化的数据结构 ----

struct SerializedItem {
    int itemType = 0;
    double zValue = 0;
    double posX = 0;
    double posY = 0;
    double rotation = 0;
    QByteArray base64Data; // QDataStream 序列化后 Base64 编码
};

struct DeserialTask {
    QByteArray base64Data;
    int itemType = 0;
    double zValue = 0;
    double posX = 0;
    double posY = 0;
    double rotation = 0;
};

// ---- 线程安全的 Worker 函数（供 QtConcurrent::mapped 使用） ----

SerializedItem serializeItemWorker(QGraphicsItem *item);
IGraphicsItem *deserializeItemWorker(const DeserialTask &task);

// ---- 工程文件类 ----

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

    // 从已序列化的 item 列表组装并保存 XML（在主线程调用）
    bool saveFromSerialized(const QString &filePath, const ProjectInfo &info,
                            const CanvasInfo &canvas,
                            const QList<SerializedItem> &items);

    // 解析 XML 文件，提取 DeserialTask 列表供并行反序列化
    bool parseForDeserialize(const QString &filePath, ProjectInfo &info,
                             CanvasInfo &canvas,
                             QList<DeserialTask> &tasks);

    // 同步版本的 save/load（内部串行，不推荐大数据量使用）
    bool save(const QString &filePath, const ProjectInfo &info,
              const CanvasInfo &canvas, const QList<QGraphicsItem *> &items);

    bool load(const QString &filePath, ProjectInfo &info, CanvasInfo &canvas,
              QList<QGraphicsItem *> &items);

    QString lastError() const { return m_lastError; }

private:
    QByteArray encrypt(const QByteArray &data, const QByteArray &key);
    QByteArray decrypt(const QByteArray &data, const QByteArray &key);

    QString m_lastError;
};

#endif // PROJECTFILE_H
