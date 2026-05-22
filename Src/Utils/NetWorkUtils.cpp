#include "NetWorkUtils.h"

#define GET_AND_EMIT(url, type)                                    \
    Http::Get(Http::URL(url),                                      \
              Http::ResponseFunc([&](Http::QResponsePtr ptrResp) { \
                  Q_EMIT requestRecv(ptrResp, type);               \
              }));

NetWorkUtils::NetWorkUtils(QObject *parent) : QObject(parent)
{
    connect(this, &NetWorkUtils::requestRecv, this,
            &NetWorkUtils::onReplyFinished);
}

NetWorkUtils::~NetWorkUtils()
{
    doStopWhile();
}

void NetWorkUtils::doHelpAbout()
{
    GET_AND_EMIT(NETWORK_ROOT_HELPABOUT, RequestHelpAbout);
}

void NetWorkUtils::doAddRip(int x, int y, const QString &path)
{
    QJsonObject jsonObj;
    jsonObj["x_resolution"] = x;
    jsonObj["y_resolution"] = y;
    jsonObj["prn_path"] = path;

    // 转换为 QJsonDocument 并输出紧凑格式的字符串
    QJsonDocument doc(jsonObj);
    QByteArray jsonString = doc.toJson(QJsonDocument::Compact);
    qDebug() << jsonString;

    Http::Get(Http::URL(NETWORK_ROOT_ADDRIP),
              Http::Parameters({ Http::Parameter("param", jsonString) }),
              Http::ResponseFunc([this](Http::QResponsePtr ptrResp) {
                  Q_EMIT requestRecv(ptrResp, RequestAddRip);
              }));
}

void NetWorkUtils::doRipStatus()
{
    GET_AND_EMIT(NETWORK_ROOT_RIPSTATUS, RequestRipStatus);
}

void NetWorkUtils::doWhileRipStatus()
{
    int id = QObject::startTimer(1000);
    m_timeoutFuncs.insert(id, [this]() { doRipStatus(); });
}

void NetWorkUtils::doStopWhile()
{
    for (auto id : m_timeoutFuncs.keys()) {
        killTimer(id);
    }
    m_timeoutFuncs.clear();
}

void NetWorkUtils::doRipVersion()
{
    GET_AND_EMIT(NETWORK_ROOT_RIPVERSION, RequestRipVersion);
}

void NetWorkUtils::timerEvent(QTimerEvent *event)
{
    int id = event->timerId();
    if (m_timeoutFuncs.contains(id)) {
        auto func = m_timeoutFuncs.value(id);
        if (func)
            func();
        else
            killTimer(id);
    }
}

void NetWorkUtils::onReplyFinished(Http::QResponsePtr ptrResponse,
                                   NetworkRequestType type)
{
    if (!ptrResponse)
        return;

    qDebug() << ptrResponse;

    QString strError = "";
    if (ptrResponse->success(strError)) {
        QJsonDocument doc = QJsonDocument::fromJson(ptrResponse->body());
        Q_EMIT requestFinished(doc, type);
    } else {
        Q_EMIT requestError(strError);
    }
}
