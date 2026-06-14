#include "ProcessService.h"
#include "atDefine.h"

#include <QTimer>
#include <QDebug>

ProcessService::ProcessService(QObject *parent) : QAtService(parent) {}

QString ProcessService::description() const { return tr("Process lifecyle service"); }

bool ProcessService::isProcessRunning(const QString &program) const
{
    const auto procs = findChildren<QProcess *>();
    for (const auto *proc : procs)
        if (proc->property("program").toString() == program
            && proc->state() == QProcess::Running)
            return true;
    return false;
}

void ProcessService::addProcess(const QString &program, const QStringList &arguments)
{
    auto *proc = new QProcess(this);
    proc->setProperty("program", program);
    proc->setProperty("arguments", arguments);
    proc->setProperty("restartCount", 0);
    proc->setProperty("stopping", false);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProcessService::onProcessFinished);
    connect(proc, &QProcess::errorOccurred, this, &ProcessService::onProcessError);
    proc->start(program, arguments);
    atDebug() << "ProcessService: started" << program;
}

void ProcessService::stopAll()
{
    for (auto *proc : findChildren<QProcess *>()) {
        proc->setProperty("stopping", true);
        proc->terminate();
    }
}

void ProcessService::setMaxRestarts(int max)  { m_maxRestarts = max; }
void ProcessService::setRestartDelay(int msec) { m_restartDelay = msec; }

void ProcessService::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    auto *proc = qobject_cast<QProcess *>(sender());
    if (!proc || proc->property("stopping").toBool()) return;
    atDebug() << "ProcessService:" << proc->property("program").toString()
              << "exited with code" << exitCode;
    scheduleRestart(proc);
}

void ProcessService::onProcessError(QProcess::ProcessError error)
{
    auto *proc = qobject_cast<QProcess *>(sender());
    if (!proc) return;
    if (error == QProcess::FailedToStart) {
        atDebug() << "ProcessService: failed to start"
                  << proc->property("program").toString();
        scheduleRestart(proc);
    }
}

void ProcessService::scheduleRestart(QProcess *process)
{
    if (process->property("stopping").toBool()) return;
    int restartCount = process->property("restartCount").toInt();
    if (m_maxRestarts >= 0 && restartCount >= m_maxRestarts) {
        qWarning() << "ProcessService: max restarts reached for"
                   << process->property("program").toString();
        emit processGiveUp(process->property("program").toString());
        return;
    }
    process->setProperty("restartCount", ++restartCount);
    const QString program = process->property("program").toString();
    const QStringList args = process->property("arguments").toStringList();
    QTimer::singleShot(m_restartDelay, process, [this, process, program, args]() {
        if (!process->property("stopping").toBool())
            process->start(program, args);
    });
}
