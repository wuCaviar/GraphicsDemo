#ifndef PROGRESSMANAGER_H
#define PROGRESSMANAGER_H

#include <QObject>
#include <QProgressBar>
#include <QLabel>
#include <QMap>
#include <QStringList>

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

// 通用进度条管理器 — 管理状态栏标签和进度条，支持多任务栈
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

    // 观察者扩展接口
    void addObserver(IProgressObserver *observer);
    void removeObserver(IProgressObserver *observer);

signals:
    void allTasksFinished();

private:
    struct Task {
        QString id;
        QString name;
        int max = 100;
        int value = 0;
    };

    void _updateDisplay();

    QLabel *m_label = nullptr;
    QProgressBar *m_bar = nullptr;
    QMap<QString, Task> m_tasks;
    QStringList m_taskStack; // first → most recent
    QList<IProgressObserver *> m_observers;
};

#endif // PROGRESSMANAGER_H
