#ifndef PROJECTFILE_H
#define PROJECTFILE_H

#include "IGraphicsItem.h"

#include <QByteArray>
#include <QDomDocument>
#include <QGraphicsItem>
#include <QList>
#include <QMap>
#include <QString>

// ---- CMYK 颜色数据（用于 XML 中精确存储） ----

struct CmykData
{
    bool hasPen = false;
    double penC = 0, penM = 0, penY = 0, penK = 0;
    bool hasBrush = false;
    double brushC = 0, brushM = 0, brushY = 0, brushK = 0;
    QMap<double, CmykColor> gradient; // position → CmykColor
};

// ---- 并发序列化/反序列化的数据结构 ----

struct SerializedItem
{
    int itemType = 0;
    double zValue = 0;
    double posX = 0;
    double posY = 0;
    double rotation = 0;
    QByteArray base64Data; // QDataStream 序列化后 Base64 编码
    CmykData cmyk; // CMYK 颜色数据（存入 XML 属性）
};

struct DeserialTask
{
    QByteArray base64Data;
    int itemType = 0;
    double zValue = 0;
    double posX = 0;
    double posY = 0;
    double rotation = 0;
    CmykData cmyk; // CMYK 颜色数据（从 XML 属性读取）
};

// Worker 产出的纯数据结构（不含 QGraphicsItem，线程安全）
struct DeserializedItem
{
    int itemType = 0;
    QByteArray binary; // 已从 Base64 解码
    double zValue = 0;
    double posX = 0;
    double posY = 0;
    double rotation = 0;
    CmykData cmyk; // CMYK 颜色数据
};

// 序列化输入快照（在主线程采集，线程安全）
struct SerializeInput
{
    int itemType = 0;
    QByteArray binary; // 已在主线程通过 serialize() 生成
    double zValue = 0;
    double posX = 0;
    double posY = 0;
    double rotation = 0;
    CmykData cmyk; // CMYK 颜色数据
};

// ---- 线程安全的 Worker 函数（供 QtConcurrent::mapped 使用） ----

SerializedItem serializeItemWorker(const SerializeInput &input);
DeserializedItem deserializeItemWorker(const DeserialTask &task);

// 在主线程从 DeserializedItem 创建 QGraphicsItem
QGraphicsItem *createItemFromDeserialized(const DeserializedItem &data);

// ---- 工程文件类 ----

class ProjectFile
{
public:
    struct ProjectInfo
    {
        QString name;
        QString version = QStringLiteral("1.0.0");
        QString author;
        QString description;
    };

    struct CanvasInfo
    {
        double width = 1920.0;
        double height = 1080.0;
        double dpi = 96.0;
    };

    // 从已序列化的 item 列表组装并保存 XML（在主线程调用）
    bool saveFromSerialized(const QString &filePath, const ProjectInfo &info,
                            const CanvasInfo &canvas, const QList<SerializedItem> &items);

    // 解析 XML 文件，提取 DeserialTask 列表供并行反序列化
    bool parseForDeserialize(const QString &filePath, ProjectInfo &info, CanvasInfo &canvas,
                             QList<DeserialTask> &tasks);

    // 同步版本的 save/load（内部串行，不推荐大数据量使用）
    bool save(const QString &filePath, const ProjectInfo &info, const CanvasInfo &canvas,
              const QList<QGraphicsItem *> &items);

    bool load(const QString &filePath, ProjectInfo &info, CanvasInfo &canvas,
              QList<QGraphicsItem *> &items);

    QString lastError() const { return m_lastError; }

private:
    QByteArray encrypt(const QByteArray &data, const QByteArray &key);
    QByteArray decrypt(const QByteArray &data, const QByteArray &key);

    QString m_lastError;
};

#endif // PROJECTFILE_H
