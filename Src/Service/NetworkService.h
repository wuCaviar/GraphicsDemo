#ifndef NETWORKSERVICE_H
#define NETWORKSERVICE_H

#include "QAtService.h"
#include "QHttp.h"
#include "NetWorkDefs.h"

#include <QJsonDocument>
#include <QMap>
#include <functional>

typedef std::function<void()> TimeoutFunc;

// HTTP 请求发送与响应解析服务（从 NetWorkUtils 提取核心逻辑）
class NetworkService : public QAtService
{
    Q_OBJECT
public:
    explicit NetworkService(QObject *parent = nullptr);
    ~NetworkService() override;

    QString serviceId() const override { return QStringLiteral("network"); }
    QString description() const override;

    void doHelpAbout();
    void doAddRip(int x, int y, const QString &path);
    void doRipStatus();
    void doWhileRipStatus();
    void doStopWhile();
    void doRipVersion();
    void doCancelRip();
    void doExit();

signals:
    void requestRecv(Http::QResponsePtr ptrResp, NetworkRequestType type);
    void requestFinished(const QJsonDocument &json, NetworkRequestType type);
    void requestError(const QString &errorString);

protected:
    void timerEvent(QTimerEvent *event) override;

private slots:
    void onReplyFinished(Http::QResponsePtr ptrResp, NetworkRequestType type);

private:
    QMap<int, TimeoutFunc> m_timeoutFuncs;
};

#endif // NETWORKSERVICE_H
