#ifndef PROCESSSERVICE_H
#define PROCESSSERVICE_H

#include "QAtService.h"

#include <QProcess>
#include <QStringList>

// 子进程生命周期管理服务（从 ProcessGuard 提取核心逻辑）
class ProcessService : public QAtService
{
    Q_OBJECT
public:
    explicit ProcessService(QObject *parent = nullptr);

    QString serviceId() const override { return QStringLiteral("process"); }
    QString description() const override;

    void addProcess(const QString &program, const QStringList &arguments = {});
    void stopAll();
    void setMaxRestarts(int max);
    void setRestartDelay(int msec);
    bool isProcessRunning(const QString &program) const;

signals:
    void processGiveUp(const QString &program);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void scheduleRestart(QProcess *process);
    int m_maxRestarts = 5;
    int m_restartDelay = 1000;
};

#endif // PROCESSSERVICE_H
