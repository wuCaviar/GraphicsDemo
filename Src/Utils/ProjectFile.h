#ifndef PROJECTFILE_H
#define PROJECTFILE_H

#include "IGraphicsItem.h"

#include <QByteArray>
#include <QDomDocument>
#include <QGraphicsItem>
#include <QList>
#include <QMap>
#include <QString>

struct CanvasInfo
{
    double width = 1920.0;
    double height = 1080.0;
    double dpi = 96.0;
};

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

// ---- Multi-canvas data bundles ----

struct CanvasSaveBundle
{
    CanvasInfo info;
    QList<SerializedItem> items;
};

struct CanvasDeserialBundle
{
    CanvasInfo info;
    QList<DeserialTask> tasks;
};

// ---- 工程文件类 ----
// ATP v2 format: one project → N canvases (each with its own size/dpi/items)
// ATP v1 backward-compatible: single <Canvas> element read as one-canvas project

class ProjectFile
{
public:
    struct ProjectInfo
    {
        QString name;
        QString version = QStringLiteral("2.0.0");
        QString author;
        QString description;
    };

    // ---- Multi-canvas save ----
    bool saveMulti(const QString &filePath, const ProjectInfo &info,
                   const QList<CanvasSaveBundle> &canvases);

    // ---- Multi-canvas parse ----
    bool parseMulti(const QString &filePath, ProjectInfo &info,
                    QList<CanvasDeserialBundle> &canvases);

    // ---- Legacy single-canvas API (delegates to multi-canvas internally) ----
    bool saveFromSerialized(const QString &filePath, const ProjectInfo &info,
                            const CanvasInfo &canvas, const QList<SerializedItem> &items);

    bool parseForDeserialize(const QString &filePath, ProjectInfo &info, CanvasInfo &canvas,
                             QList<DeserialTask> &tasks);

    bool save(const QString &filePath, const ProjectInfo &info, const CanvasInfo &canvas,
              const QList<QGraphicsItem *> &items);

    bool load(const QString &filePath, ProjectInfo &info, CanvasInfo &canvas,
              QList<QGraphicsItem *> &items);

    QString lastError() const { return m_lastError; }

private:
    void _writeCanvasToXml(QDomDocument &doc, QDomElement &parent, int index,
                           const CanvasSaveBundle &bundle);
    void _writeSingleCanvasToXml(QDomDocument &doc, QDomElement &parent, const CanvasInfo &info,
                                 const QList<SerializedItem> &items);
    bool _parseCanvasElement(const QDomElement &el, CanvasDeserialBundle &bundle);
    QByteArray encrypt(const QByteArray &data, const QByteArray &key);
    QByteArray decrypt(const QByteArray &data, const QByteArray &key);

    QString m_lastError;
};

#endif // PROJECTFILE_H
