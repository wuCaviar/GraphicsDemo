#ifndef NETWORKUTILS_H
#define NETWORKUTILS_H

#include <QObject>
#include <QThread>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "../NetWork/QHttp.h"

// 独立线程用于专门跑网络接口
class NetWorkUtils : public QObject
{
    Q_OBJECT
public:
    explicit NetWorkUtils(QObject *parent = nullptr);
    ~NetWorkUtils();

    // 启动工作线程
    void start();
    // 停止工作线程
    void stop();

public:
    // /helpabout
    void doHelpAbout();

    // /addrip
    void doAddRip(int x, int y, const QString &path);

    // /ripstatus
    void doRipStatus();

    // /ripVersion
    void doRipVersion();

signals:
    // 请求成功信号（响应体数据）
    void requestFinished(const QJsonDocument &json);
    // 请求错误信号
    void requestError(const QString &errorString);

    void requestRecv(Http::QResponsePtr ptrResp);

private slots:
    // 在工作线程中初始化 manager（必须在正确线程中调用）
    void init();

    void onReplyFinished(Http::QResponsePtr ptrResp);

protected:
    template<typename... Ts>
    void doGet(Ts &&...ts);

private:
    QThread m_workerThread;
    QProcess *m_pExeProcess;
};

template<typename... Ts>
void NetWorkUtils::doGet(Ts &&...ts)
{
    Http::Get(ts...);
}

#endif // NETWORKUTILS_H
