#ifndef PROJECTDOCUMENT_H
#define PROJECTDOCUMENT_H

#include "ProjectFile.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QFileInfo>

class QAtCanvasPage;

// 单个画布的工程状态（替代 MainWindow::TabProjectState）
struct CanvasState
{
    bool modified = false;
};

// 工程文档模型 —— 所有工程状态的唯一数据源
// 替代 MainWindow 中散落的 m_projectPath / m_projectModified / m_tabStates
// 以及 AppContext 中重复的 m_projectPath / m_projectModified
class ProjectDocument : public QObject
{
    Q_OBJECT

public:
    explicit ProjectDocument(QObject *parent = nullptr);
    ~ProjectDocument() override = default;

    // ---- 路径管理 ----
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path);
    bool isNamed() const { return !m_filePath.isEmpty(); }
    QString displayName() const; // "未命名工程" 或文件名
    QFileInfo fileInfo() const { return QFileInfo(m_filePath); }

    // ---- 修改追踪 ----
    bool isModified() const; // 任一画布有未保存变更
    bool isCanvasModified(QAtCanvasPage *page) const;
    void markCanvasModified(QAtCanvasPage *page, bool modified = true);
    void markAllClean();
    bool hasAnyContent() const; // 是否有任何画布包含图元

    // ---- 画布状态管理 ----
    void registerCanvas(QAtCanvasPage *page);
    void unregisterCanvas(QAtCanvasPage *page);
    CanvasState canvasState(QAtCanvasPage *page) const;
    QList<QAtCanvasPage *> canvases() const;

    // ---- 备份链 ----
    int maxBackups() const { return m_maxBackups; }
    void setMaxBackups(int n) { m_maxBackups = qMax(0, n); }
    void rotateBackups() const; // xxx.atp → xxx.atp1 → xxx.atp2 → ...
    static void rotateBackups(const QString &filePath, int maxBackups = 3);

    // ---- 序列化收集 (在主线程调用) ----
    QList<CanvasSaveBundle> collectBundles() const;

signals:
    void filePathChanged(const QString &path);
    void modifiedChanged(bool modified);
    void displayNameChanged(const QString &name);

private:
    QString m_filePath;
    int m_maxBackups = 3;
    QMap<QAtCanvasPage *, CanvasState> m_canvasStates;
};

#endif // PROJECTDOCUMENT_H
