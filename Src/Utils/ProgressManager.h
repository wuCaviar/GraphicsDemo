#ifndef PROGRESSMANAGER_H
#define PROGRESSMANAGER_H

#include <QObject>
#include <QProgressBar>
#include <QLabel>
#include <QMap>
#include <QStringList>
#include <QTimer>
#include <QList>

// 进度任务生命周期观察者接口 — 后续功能通过实现此接口监听进度事件
class IProgressObserver
{
public:
    virtual ~IProgressObserver() = default;
    virtual void onTaskStarted(const QString &taskId, const QString &name) = 0;
    virtual void onTaskProgress(const QString &taskId, int value, int max) = 0;
    virtual void onTaskFinished(const QString &taskId) = 0;
    virtual void onAllTasksFinished() = 0;
};

// 通用进度条管理器 — 管理状态栏标签和进度条，支持多任务栈与历史查询
class ProgressManager : public QObject
{
    Q_OBJECT
public:
    explicit ProgressManager(QObject *parent = nullptr);

    // 创建并返回状态栏控件（调用方负责添加到 statusBar）
    QLabel *label() const { return m_label; }
    QProgressBar *bar() const { return m_bar; }

    // 任务管理 API
    QString startTask(const QString &name, int max = 100);
    void updateTask(const QString &taskId, int value);
    void finishTask(const QString &taskId);
    void cancelTask(const QString &taskId);

    // 重置所有任务，进度条归零
    void resetAll();

    // 查询
    bool isActive() const;
    QString activeTaskName() const;
    bool hasActiveTasks() const;
    bool hasFinishedTasks() const;

    // 焦点任务：允许状态栏显示非栈顶任务
    void setFocusTask(const QString &taskId);
    QString focusTaskId() const;
    void clearFocus();

    // 供弹窗查询的数据结构
    struct TaskInfo
    {
        QString id;
        QString name;
        int value = 0;
        int max = 100;
        bool finished = false;
    };
    QList<TaskInfo> activeTaskList() const;
    QList<TaskInfo> finishedTaskList() const;

    // 观察者扩展接口
    void addObserver(IProgressObserver *observer);
    void removeObserver(IProgressObserver *observer);

signals:
    void allTasksFinished();
    void taskHistoryChanged(); // 弹窗刷新信号
    void focusTaskChanged(); // 焦点切换信号

private:
    struct Task
    {
        QString id;
        QString name;
        int max = 100;
        int value = 0;
    };

    struct FinishedEntry
    {
        QString id;
        QString name;
        QTimer *expiryTimer = nullptr;
    };

    void _updateDisplay();
    void _removeFinishedEntry(const QString &taskId);

    QLabel *m_label = nullptr;
    QProgressBar *m_bar = nullptr;
    QMap<QString, Task> m_tasks;
    QStringList m_taskStack;
    QList<FinishedEntry> m_finishedHistory;
    QString m_focusTaskId;
    QList<IProgressObserver *> m_observers;
};

#endif // PROGRESSMANAGER_H
