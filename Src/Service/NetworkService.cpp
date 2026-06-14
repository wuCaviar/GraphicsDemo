#include "NetworkService.h"

#include <QTimerEvent>
#include <QJsonObject>

#define GET_AND_EMIT(url, type) \
    Http::Get(Http::URL(url), Http::ResponseFunc([this](Http::QResponsePtr ptrResp) { \
                  Q_EMIT requestRecv(ptrResp, type); \
              }));

NetworkService::NetworkService(QObject *parent) : QAtService(parent)
{
    connect(this, &NetworkService::requestRecv, this, &NetworkService::onReplyFinished);
}

NetworkService::~NetworkService() { doStopWhile(); }

QString NetworkService::description() const { return tr("HTTP request service"); }

void NetworkService::doHelpAbout()    { GET_AND_EMIT(NETWORK_ROOT_HELPABOUT, RequestHelpAbout); }
void NetworkService::doRipStatus()    { GET_AND_EMIT(NETWORK_ROOT_RIPSTATUS, RequestRipStatus); }
void NetworkService::doRipVersion()   { GET_AND_EMIT(NETWORK_ROOT_RIPVERSION, RequestRipVersion); }
void NetworkService::doExit()         { GET_AND_EMIT(NETWORK_ROOT_EXIT, RequestExit); }
void NetworkService::doCancelRip()    { GET_AND_EMIT(NETWORK_ROOT_CANCELRIP, RequestCancelRip); }

void NetworkService::doAddRip(int x, int y, const QString &path)
{
    QJsonObject jsonObj;
    jsonObj["x_resolution"] = x;
    jsonObj["y_resolution"] = y;
    jsonObj["prn_path"] = path;
    QByteArray jsonString = QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);
    Http::Get(Http::URL(NETWORK_ROOT_ADDRIP),
              Http::Parameters({Http::Parameter("param", jsonString)}),
              Http::ResponseFunc([this](Http::QResponsePtr ptrResp) {
                  Q_EMIT requestRecv(ptrResp, RequestAddRip);
              }));
}

void NetworkService::doWhileRipStatus()
{
    int id = startTimer(3000);
    m_timeoutFuncs.insert(id, [this]() { doRipStatus(); });
}

void NetworkService::doStopWhile()
{
    for (auto id : m_timeoutFuncs.keys()) killTimer(id);
    m_timeoutFuncs.clear();
}

void NetworkService::timerEvent(QTimerEvent *event)
{
    int id = event->timerId();
    if (m_timeoutFuncs.contains(id)) {
        auto func = m_timeoutFuncs.value(id);
        if (func) func(); else killTimer(id);
    }
}

void NetworkService::onReplyFinished(Http::QResponsePtr ptrResponse, NetworkRequestType type)
{
    if (!ptrResponse) return;
    QString strError;
    if (ptrResponse->success(strError)) {
        QJsonDocument doc = QJsonDocument::fromJson(ptrResponse->body());
        Q_EMIT requestFinished(doc, type);
    } else {
        Q_EMIT requestError(strError);
    }
}
