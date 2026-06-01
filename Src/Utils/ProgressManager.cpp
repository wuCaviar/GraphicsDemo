#include "ProgressManager.h"

#include <QUuid>
#include <QApplication>

ProgressManager::ProgressManager(QObject *parent) : QObject(parent)
{
    m_label = new QLabel(tr("Ready"));
    m_label->setMinimumWidth(80);
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

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

    m_bar->setRange(0, max);
    m_bar->setValue(0);
    m_bar->show();
    _updateDisplay();

    for (auto *obs : m_observers)
        obs->onTaskStarted(id, name);

    return id;
}

void ProgressManager::updateTask(const QString &taskId, int value)
{
    if (!m_tasks.contains(taskId))
        return;

    Task &task = m_tasks[taskId];
    task.value = value;

    // Only update display if this is the active (topmost) task
    if (!m_taskStack.isEmpty() && m_taskStack.last() == taskId) {
        m_bar->setValue(value);
        _updateDisplay();
    }

    for (auto *obs : m_observers)
        obs->onTaskProgress(taskId, value, task.max);
}

void ProgressManager::finishTask(const QString &taskId)
{
    if (!m_tasks.contains(taskId))
        return;

    m_tasks.remove(taskId);
    m_taskStack.removeAll(taskId);

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
        // Show the previous task
        const QString &prevId = m_taskStack.last();
        const Task &prev = m_tasks[prevId];
        m_bar->setValue(prev.value);
        _updateDisplay();
        for (auto *obs : m_observers)
            obs->onTaskFinished(taskId);
    }
}

void ProgressManager::cancelTask(const QString &taskId)
{
    if (!m_tasks.contains(taskId))
        return;

    m_tasks.remove(taskId);
    m_taskStack.removeAll(taskId);

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
}

void ProgressManager::resetAll()
{
    m_tasks.clear();
    m_taskStack.clear();
    m_bar->setValue(0);
    m_bar->hide();
    m_label->setText(tr("Ready"));
}

bool ProgressManager::isActive() const
{
    return !m_taskStack.isEmpty();
}

QString ProgressManager::activeTaskName() const
{
    if (m_taskStack.isEmpty())
        return { };
    return m_tasks.value(m_taskStack.last()).name;
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
    if (m_taskStack.isEmpty())
        return;

    const Task &task = m_tasks.value(m_taskStack.last());
    m_label->setText(task.name);
}
