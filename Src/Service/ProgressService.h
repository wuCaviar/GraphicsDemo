#ifndef PROGRESSSERVICE_H
#define PROGRESSSERVICE_H

#include "QAtService.h"

#include <QMap>
#include <QStringList>
#include <QList>
#include <QTimer>

// 多任务进度追踪服务（从 ProgressManager 提取核心逻辑）
// 不持有 UI 控件，通过信号通知进度变更
class ProgressService : public QAtService
{
    Q_OBJECT
public:
    explicit ProgressService(QObject *parent = nullptr);

    QString serviceId() const override { return QStringLiteral("progress"); }
    QString description() const override;

    // Task lifecycle
    QString startTask(const QString &name, int max = 100);
    void    updateTask(const QString &taskId, int value);
    void    finishTask(const QString &taskId);
    void    cancelTask(const QString &taskId);
    void    resetAll();

    // Query
    bool    isActive() const;
    bool    hasActiveTasks() const;
    bool    hasFinishedTasks() const;
    QString activeTaskName() const;

    // Focus task
    void    setFocusTask(const QString &taskId);
    QString focusTaskId() const;
    void    clearFocus();

    struct TaskInfo {
        QString id, name;
        int value = 0, max = 100;
        bool finished = false;
    };
    QList<TaskInfo> activeTaskList() const;
    QList<TaskInfo> finishedTaskList() const;

signals:
    void taskStarted(const QString &taskId, const QString &name);
    void taskProgress(const QString &taskId, int value, int max);
    void taskFinished(const QString &taskId);
    void allTasksFinished();
    void taskHistoryChanged();
    void focusTaskChanged();

private:
    struct Task { QString id, name; int max = 100, value = 0; };
    struct FinishedEntry { QString id, name; QTimer *expiryTimer = nullptr; };

    void removeFinishedEntry(const QString &taskId);
    void updateFocus();

    QMap<QString, Task> m_tasks;
    QStringList         m_taskStack;
    QList<FinishedEntry> m_finishedHistory;
    QString             m_focusTaskId;
};

#endif // PROGRESSSERVICE_H
