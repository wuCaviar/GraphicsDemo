#ifndef RECOVERYMANAGER_H
#define RECOVERYMANAGER_H

#include "ProjectFile.h"
#include "SessionFile.h"

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QList>

// 恢复管理器 —— 管理恢复区文件 (Level 2 + Level 3)
// Level 2: 定时自动保存 → recovery/autosave_{hash}_{timestamp}.atre
// Level 3: 退出快照 → recovery/quit.atre
//
// 设计理念: 关闭时静默写入 quit.atre，下次启动时检测残留 → 说明上次异常退出
class RecoveryManager : public QObject
{
    Q_OBJECT

public:
    static RecoveryManager &instance();

    // ---- 退出快照 (Level 3) ----
    // 写入全部画布数据 + 会话状态到 recovery/quit.atre
    // 使用原子写入 (temp + rename)
    bool saveQuitSnapshot(const QList<CanvasSaveBundle> &bundles, const QString &projectPath,
                          const SessionInfo &session);

    bool hasQuitSnapshot() const;
    // 加载退出快照；若 isValid=false 说明无快照或损坏
    struct QuitSnapshot
    {
        QList<CanvasDeserialBundle> canvases;
        QString projectPath;
        SessionInfo session;
        QDateTime timestamp;
        bool isValid = false;
    };
    QuitSnapshot loadQuitSnapshot() const;
    void discardQuitSnapshot();

    // ---- 定时自动保存 (Level 2) ----
    bool saveAutoSnapshot(const QList<CanvasSaveBundle> &bundles, const QString &projectHash);

    struct AutoSnapshotEntry
    {
        QString filePath;
        QDateTime timestamp;
        int canvasCount = 0;
        int itemCount = 0;
    };
    QList<AutoSnapshotEntry> listAutoSnapshots() const;
    void cleanAutoSnapshots(int keepHours = 24);

    // ---- 路径 ----
    QString recoveryDir() const;
    QString quitSnapshotPath() const;

signals:
    void autoSaveCompleted();
    void autoSaveFailed(const QString &error);

private:
    RecoveryManager();
    ~RecoveryManager() override = default;
    Q_DISABLE_COPY(RecoveryManager)

    bool ensureRecoveryDir() const;
    QString m_recoveryDir;
};

#endif // RECOVERYMANAGER_H
