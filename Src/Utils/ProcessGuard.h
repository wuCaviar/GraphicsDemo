#ifndef PROCESSGUARD_H
#define PROCESSGUARD_H

#include <QObject>
#include <QProcess>
#include <QStringList>

class ProcessGuard : public QObject
{
    Q_OBJECT
public:
    explicit ProcessGuard(QObject *parent = nullptr);
    ~ProcessGuard() override = default;

    // 添加需要守护的进程
    void addProcess(const QString &program, const QStringList &arguments = {});
    // 停止所有守护进程（主动停止，不再重启）
    void stopAll();
    // 设置最大重启次数（默认5，-1表示无限）
    void setMaxRestarts(int max);
    // 设置重启延时（毫秒，默认1000）
    void setRestartDelay(int msec);
    // 如果同一个程序被添加了多次（例如启动多个实例），只要任意一个正在运行，即返回 true。
    // 处于延迟重启等待期的进程（已退出但定时器尚未触发 start()）被视为 未运行，返回 false。
    // 如果从未添加过该程序，返回 false。
    bool isProcessRunning(const QString &program) const;

signals:
    // 某个进程达到最大重启次数，放弃守护
    void processGiveUp(const QString &program);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void scheduleRestart(QProcess *process);

    int m_maxRestarts = 5;
    int m_restartDelay = 1000; // 1秒
};

#endif // PROCESSGUARD_H
