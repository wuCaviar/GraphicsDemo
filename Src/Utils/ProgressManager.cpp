#include "ProgressManager.h"

#include <QUuid>
#include <QApplication>

static constexpr int kFinishedExpiryMs = 5000;
static constexpr int kMaxFinishedEntries = 5;

ProgressManager::ProgressManager(QObject *parent) : QObject(parent)
{
    m_label = new QLabel(tr("Ready"));

    m_bar = new QProgressBar();
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    m_bar->setFormat("%p%");
    m_bar->hide();
}

QString ProgressManager::startTask(const QString &name, int max)
{
    const QString id = QUuid::createUuid().toString(QUuid::Id128);

    Task task;
    task.id = id;
    task.name = name;
    task.max = max;
    task.value = 0;
    m_tasks.insert(id, task);
    m_taskStack.append(id);

    // 新任务开始时清除焦点，回归栈顶显示
    m_focusTaskId.clear();

    m_bar->setRange(0, max);
    m_bar->setValue(0);
    m_bar->show();
    _updateDisplay();

    for (auto *obs : m_observers)
        obs->onTaskStarted(id, name);

    emit taskHistoryChanged();
    emit focusTaskChanged();
    return id;
}

void ProgressManager::updateTask(const QString &taskId, int value)
{
    if (!m_tasks.contains(taskId))
        return;

    Task &task = m_tasks[taskId];
    task.value = value;

    // 仅当前显示的任务才更新进度条
    const QString displayedId =
        m_focusTaskId.isEmpty()
            ? (m_taskStack.isEmpty() ? QString() : m_taskStack.last())
            : m_focusTaskId;
    if (displayedId == taskId) {
        m_bar->setValue(value);
        _updateDisplay();
    }

    for (auto *obs : m_observers)
        obs->onTaskProgress(taskId, value, task.max);

    emit taskHistoryChanged(); // 通知弹窗刷新进度值
}

void ProgressManager::finishTask(const QString &taskId)
{
    if (!m_tasks.contains(taskId))
        return;

    const Task task = m_tasks.value(taskId);
    m_tasks.remove(taskId);
    m_taskStack.removeAll(taskId);

    // 加入已完成历史（最多 kMaxFinishedEntries 条）
    if (m_finishedHistory.size() >= kMaxFinishedEntries) {
        FinishedEntry &oldest = m_finishedHistory.first();
        if (oldest.expiryTimer) {
            oldest.expiryTimer->stop();
            oldest.expiryTimer->deleteLater();
        }
        m_finishedHistory.removeFirst();
    }

    FinishedEntry entry;
    entry.id = task.id;
    entry.name = task.name;
    entry.expiryTimer = new QTimer(this);
    entry.expiryTimer->setSingleShot(true);
    connect(entry.expiryTimer, &QTimer::timeout, this,
            [this, id = task.id]() { _removeFinishedEntry(id); });
    entry.expiryTimer->start(kFinishedExpiryMs);
    m_finishedHistory.append(entry);

    // 如果焦点任务完成了，清除焦点
    if (m_focusTaskId == taskId)
        m_focusTaskId.clear();

    if (m_taskStack.isEmpty()) {
        m_bar->setValue(0);
        m_bar->hide();
        m_label->setText(tr("Ready"));
        emit allTasksFinished();

        for (auto *obs : m_observers)
            obs->onTaskFinished(taskId);
        for (auto *obs : m_observers)
            obs->onAllTasksFinished();
    } else {
        const QString &prevId = m_taskStack.last();
        const Task &prev = m_tasks[prevId];
        m_bar->setValue(prev.value);
        _updateDisplay();
        for (auto *obs : m_observers)
            obs->onTaskFinished(taskId);
    }

    emit taskHistoryChanged();
    emit focusTaskChanged();
}

void ProgressManager::cancelTask(const QString &taskId)
{
    if (!m_tasks.contains(taskId))
        return;

    m_tasks.remove(taskId);
    m_taskStack.removeAll(taskId);

    if (m_focusTaskId == taskId)
        m_focusTaskId.clear();

    if (m_taskStack.isEmpty()) {
        m_bar->setValue(0);
        m_bar->hide();
        m_label->setText(tr("Ready"));
        emit allTasksFinished();
        for (auto *obs : m_observers)
            obs->onTaskFinished(taskId);
        for (auto *obs : m_observers)
            obs->onAllTasksFinished();
    } else {
        const QString &prevId = m_taskStack.last();
        const Task &prev = m_tasks[prevId];
        m_bar->setValue(prev.value);
        _updateDisplay();
        for (auto *obs : m_observers)
            obs->onTaskFinished(taskId);
    }

    emit taskHistoryChanged();
    emit focusTaskChanged();
}

void ProgressManager::resetAll()
{
    for (auto &entry : m_finishedHistory) {
        if (entry.expiryTimer) {
            entry.expiryTimer->stop();
            entry.expiryTimer->deleteLater();
        }
    }
    m_finishedHistory.clear();
    m_tasks.clear();
    m_taskStack.clear();
    m_focusTaskId.clear();
    m_bar->setValue(0);
    m_bar->hide();
    m_label->setText(tr("Ready"));
    emit taskHistoryChanged();
}

bool ProgressManager::isActive() const
{
    return !m_taskStack.isEmpty();
}

bool ProgressManager::hasActiveTasks() const
{
    return !m_taskStack.isEmpty();
}

bool ProgressManager::hasFinishedTasks() const
{
    return !m_finishedHistory.isEmpty();
}

QString ProgressManager::activeTaskName() const
{
    if (!m_focusTaskId.isEmpty() && m_tasks.contains(m_focusTaskId))
        return m_tasks.value(m_focusTaskId).name;
    if (m_taskStack.isEmpty())
        return { };
    return m_tasks.value(m_taskStack.last()).name;
}

void ProgressManager::setFocusTask(const QString &taskId)
{
    if (m_tasks.contains(taskId)) {
        m_focusTaskId = taskId;
        const Task &task = m_tasks[taskId];
        m_bar->setRange(0, task.max);
        m_bar->setValue(task.value);
        m_bar->show();
        _updateDisplay();
        emit focusTaskChanged();
    }
}

QString ProgressManager::focusTaskId() const
{
    return m_focusTaskId;
}

void ProgressManager::clearFocus()
{
    if (m_focusTaskId.isEmpty())
        return;
    m_focusTaskId.clear();
    if (m_taskStack.isEmpty()) {
        m_bar->setValue(0);
        m_bar->hide();
        m_label->setText(tr("Ready"));
    } else {
        const Task &top = m_tasks.value(m_taskStack.last());
        m_bar->setRange(0, top.max);
        m_bar->setValue(top.value);
        m_bar->show();
        _updateDisplay();
    }
    emit focusTaskChanged();
}

QList<ProgressManager::TaskInfo> ProgressManager::activeTaskList() const
{
    QList<TaskInfo> result;
    for (const QString &id : m_taskStack) {
        const Task &t = m_tasks.value(id);
        TaskInfo info;
        info.id = t.id;
        info.name = t.name;
        info.value = t.value;
        info.max = t.max;
        info.finished = false;
        result.append(info);
    }
    return result;
}

QList<ProgressManager::TaskInfo> ProgressManager::finishedTaskList() const
{
    QList<TaskInfo> result;
    for (const auto &entry : m_finishedHistory) {
        TaskInfo info;
        info.id = entry.id;
        info.name = entry.name;
        info.value = 100;
        info.max = 100;
        info.finished = true;
        result.append(info);
    }
    return result;
}

void ProgressManager::addObserver(IProgressObserver *observer)
{
    if (!m_observers.contains(observer))
        m_observers.append(observer);
}

void ProgressManager::removeObserver(IProgressObserver *observer)
{
    m_observers.removeAll(observer);
}

void ProgressManager::_updateDisplay()
{
    if (!m_focusTaskId.isEmpty() && m_tasks.contains(m_focusTaskId)) {
        m_label->setText(m_tasks.value(m_focusTaskId).name);
        return;
    }
    if (m_taskStack.isEmpty())
        return;
    const Task &task = m_tasks.value(m_taskStack.last());
    m_label->setText(task.name);
}

void ProgressManager::_removeFinishedEntry(const QString &taskId)
{
    for (int i = 0; i < m_finishedHistory.size(); ++i) {
        if (m_finishedHistory[i].id == taskId) {
            if (m_finishedHistory[i].expiryTimer)
                m_finishedHistory[i].expiryTimer->deleteLater();
            m_finishedHistory.removeAt(i);
            emit taskHistoryChanged();
            return;
        }
    }
}
