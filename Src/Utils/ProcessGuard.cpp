#include "ProcessGuard.h"
#include <QTimer>
#include <QDebug>
#include <QVariant>

ProcessGuard::ProcessGuard(QObject *parent) : QObject(parent) { }

bool ProcessGuard::isProcessRunning(const QString &program) const
{
    const auto procs = findChildren<QProcess *>();
    for (const QProcess *proc : procs) {
        if (proc->property("program").toString() == program) {
            if (proc->state() == QProcess::Running)
                return true;
        }
    }
    return false;
}

void ProcessGuard::addProcess(const QString &program, const QStringList &arguments)
{
    auto *proc = new QProcess(this);
    proc->setProperty("program", program);
    proc->setProperty("arguments", arguments);
    proc->setProperty("restartCount", 0);
    proc->setProperty("stopping", false);

    // 连接信号
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &ProcessGuard::onProcessFinished);
    connect(proc, &QProcess::errorOccurred, this, &ProcessGuard::onProcessError);

    proc->start(program, arguments);
    qDebug() << "ProcessGuard: started" << program;
}

void ProcessGuard::stopAll()
{
    // 找到所有子QProcess，标记为主动停止
    const auto procs = findChildren<QProcess *>();
    for (auto *proc : procs) {
        proc->setProperty("stopping", true);
        proc->terminate(); // 尝试优雅退出
        // 如果程序不响应terminate，析构时会kill，通常足够
    }
    qDebug() << "ProcessGuard: stopping all guarded processes";
}

void ProcessGuard::setMaxRestarts(int max)
{
    m_maxRestarts = max;
}

void ProcessGuard::setRestartDelay(int msec)
{
    m_restartDelay = msec;
}

void ProcessGuard::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    auto *proc = qobject_cast<QProcess *>(sender());
    if (!proc)
        return;

    const bool stopping = proc->property("stopping").toBool();
    if (stopping) {
        qDebug() << "ProcessGuard: process" << proc->property("program").toString()
                 << "stopped intentionally, won't restart";
        return;
    }

    // 意外退出（包括正常退出和崩溃），尝试重启
    qDebug() << "ProcessGuard: process" << proc->property("program").toString()
             << "exited unexpectedly with code" << exitCode
             << (exitStatus == QProcess::CrashExit ? "(crash)" : "(normal)");
    scheduleRestart(proc);
}

void ProcessGuard::onProcessError(QProcess::ProcessError error)
{
    auto *proc = qobject_cast<QProcess *>(sender());
    if (!proc)
        return;

    // 只对启动失败进行重启处理，其他错误交给 finished 信号
    if (error == QProcess::FailedToStart) {
        qDebug() << "ProcessGuard: failed to start" << proc->property("program").toString();
        scheduleRestart(proc);
    }
}

void ProcessGuard::scheduleRestart(QProcess *process)
{
    const bool stopping = process->property("stopping").toBool();
    if (stopping)
        return;

    int restartCount = process->property("restartCount").toInt();
    if (m_maxRestarts >= 0 && restartCount >= m_maxRestarts) {
        qWarning() << "ProcessGuard: process" << process->property("program").toString()
                   << "reached max restarts, giving up";
        emit processGiveUp(process->property("program").toString());
        return;
    }

    ++restartCount;
    process->setProperty("restartCount", restartCount);

    // 延迟重启，将 process 作为上下文对象，防止 use-after-free
    const QString program = process->property("program").toString();
    const QStringList args = process->property("arguments").toStringList();
    QTimer::singleShot(m_restartDelay, process, [this, process, program, args]() {
        if (process->property("stopping").toBool())
            return;
        qDebug() << "ProcessGuard: restarting" << program;
        process->start(program, args);
    });
}