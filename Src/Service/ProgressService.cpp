#include "ProgressService.h"
#include <QUuid>

static constexpr int kFinishedExpiryMs = 5000;
static constexpr int kMaxFinishedEntries = 5;

ProgressService::ProgressService(QObject *parent) : QAtService(parent) {}

QString ProgressService::description() const { return tr("Progress tracking service"); }

QString ProgressService::startTask(const QString &name, int max)
{
    const QString id = QUuid::createUuid().toString(QUuid::Id128);
    Task t{id, name, max, 0};
    m_tasks.insert(id, t);
    m_taskStack.append(id);
    m_focusTaskId.clear();
    emit taskStarted(id, name);
    emit taskHistoryChanged();
    emit focusTaskChanged();
    return id;
}

void ProgressService::updateTask(const QString &taskId, int value)
{
    if (!m_tasks.contains(taskId)) return;
    m_tasks[taskId].value = value;
    const QString displayedId = m_focusTaskId.isEmpty()
        ? (m_taskStack.isEmpty() ? QString() : m_taskStack.last()) : m_focusTaskId;
    if (displayedId == taskId)
        emit taskProgress(taskId, value, m_tasks[taskId].max);
    emit taskHistoryChanged();
}

void ProgressService::finishTask(const QString &taskId)
{
    if (!m_tasks.contains(taskId)) return;
    const Task task = m_tasks.take(taskId);
    m_taskStack.removeAll(taskId);

    if (m_finishedHistory.size() >= kMaxFinishedEntries) {
        FinishedEntry &oldest = m_finishedHistory.first();
        if (oldest.expiryTimer) { oldest.expiryTimer->stop(); oldest.expiryTimer->deleteLater(); }
        m_finishedHistory.removeFirst();
    }
    FinishedEntry entry{task.id, task.name};
    entry.expiryTimer = new QTimer(this);
    entry.expiryTimer->setSingleShot(true);
    connect(entry.expiryTimer, &QTimer::timeout, this,
            [this, id = task.id]() { removeFinishedEntry(id); });
    entry.expiryTimer->start(kFinishedExpiryMs);
    m_finishedHistory.append(entry);

    if (m_focusTaskId == taskId) m_focusTaskId.clear();

    if (m_taskStack.isEmpty()) {
        emit taskFinished(taskId);
        emit allTasksFinished();
    } else {
        emit taskFinished(taskId);
    }
    emit taskHistoryChanged();
    emit focusTaskChanged();
}

void ProgressService::cancelTask(const QString &taskId)
{
    if (!m_tasks.contains(taskId)) return;
    m_tasks.remove(taskId);
    m_taskStack.removeAll(taskId);
    if (m_focusTaskId == taskId) m_focusTaskId.clear();

    if (m_taskStack.isEmpty()) emit allTasksFinished();
    emit taskHistoryChanged();
    emit focusTaskChanged();
}

void ProgressService::resetAll()
{
    for (auto &e : m_finishedHistory)
        if (e.expiryTimer) { e.expiryTimer->stop(); e.expiryTimer->deleteLater(); }
    m_finishedHistory.clear();
    m_tasks.clear();
    m_taskStack.clear();
    m_focusTaskId.clear();
    emit taskHistoryChanged();
}

bool ProgressService::isActive() const            { return !m_taskStack.isEmpty(); }
bool ProgressService::hasActiveTasks() const       { return !m_taskStack.isEmpty(); }
bool ProgressService::hasFinishedTasks() const     { return !m_finishedHistory.isEmpty(); }

QString ProgressService::activeTaskName() const
{
    if (!m_focusTaskId.isEmpty() && m_tasks.contains(m_focusTaskId))
        return m_tasks[m_focusTaskId].name;
    if (m_taskStack.isEmpty()) return {};
    return m_tasks[m_taskStack.last()].name;
}

void ProgressService::setFocusTask(const QString &taskId)
{
    if (m_tasks.contains(taskId)) { m_focusTaskId = taskId; emit focusTaskChanged(); }
}

QString ProgressService::focusTaskId() const       { return m_focusTaskId; }

void ProgressService::clearFocus()
{
    if (m_focusTaskId.isEmpty()) return;
    m_focusTaskId.clear();
    emit focusTaskChanged();
}

QList<ProgressService::TaskInfo> ProgressService::activeTaskList() const
{
    QList<TaskInfo> result;
    for (const auto &id : m_taskStack) {
        const auto &t = m_tasks[id];
        result.append({t.id, t.name, t.value, t.max, false});
    }
    return result;
}

QList<ProgressService::TaskInfo> ProgressService::finishedTaskList() const
{
    QList<TaskInfo> result;
    for (const auto &e : m_finishedHistory)
        result.append({e.id, e.name, 100, 100, true});
    return result;
}

void ProgressService::removeFinishedEntry(const QString &taskId)
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
