#ifndef NETWORKUTILS_H
#define NETWORKUTILS_H

#include <QObject>
#include <QThread>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimerEvent>

#include <functional>

#include "QHttp.h"
#include "NetWorkDefs.h"

typedef std::function<void()> TimeoutFunc;

class NetWorkUtils : public QObject
{
    Q_OBJECT
public:
    explicit NetWorkUtils(QObject *parent = nullptr);
    ~NetWorkUtils();

public:
    // /helpabout
    void doHelpAbout();

    // /addrip
    void doAddRip(int x, int y, const QString &path);

    // /ripstatus
    void doRipStatus();

    void doWhileRipStatus();

    void doStopWhile();

    // /ripVersion
    void doRipVersion();

    // /cancelRip
    void doCancelRip();

    // /exit
    void doExit();

protected:
    virtual void timerEvent(QTimerEvent *event) override;

signals:
    // 请求成功信号（响应体数据）
    void requestFinished(const QJsonDocument &json, NetworkRequestType type);
    // 请求错误信号
    void requestError(const QString &errorString);

    void requestRecv(Http::QResponsePtr ptrResp, NetworkRequestType type);

private slots:
    void onReplyFinished(Http::QResponsePtr ptrResp, NetworkRequestType type);

private:
    QMap<int, TimeoutFunc> m_timeoutFuncs;
};

#endif // NETWORKUTILS_H
