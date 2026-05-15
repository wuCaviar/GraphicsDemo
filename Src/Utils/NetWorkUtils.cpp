#include "NetWorkUtils.h"
#include "NetWorkDefs.h"

NetWorkUtils::NetWorkUtils(QObject *parent) : QObject(parent) { }

NetWorkUtils::~NetWorkUtils()
{
    stop();

    if (m_pExeProcess->state() != QProcess::NotRunning) {
        qDebug() << "正在结束外部程序...";
        m_pExeProcess->terminate();
        if (!m_pExeProcess->waitForFinished(3000)) {
            qWarning() << "强制结束";
            m_pExeProcess->kill();
            m_pExeProcess->waitForFinished(3000);
        }
    }

    m_pExeProcess = nullptr;
    delete m_pExeProcess;
}

void NetWorkUtils::start()
{
    // 移动到线程并启动
    moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, this, &QObject::deleteLater);
    // 线程启动后，在正确的线程中初始化 manager
    connect(&m_workerThread, &QThread::started, this, &NetWorkUtils::init);
    connect(this, &NetWorkUtils::requestRecv, this,
            &NetWorkUtils::onReplyFinished);
    m_workerThread.start();
}

void NetWorkUtils::stop()
{
    if (m_workerThread.isRunning()) {
        m_workerThread.quit();
        m_workerThread.wait();
    }
}

void NetWorkUtils::doHelpAbout()
{
    Http::Get(Http::URL(NETWORK_ROOT_HELPABOUT),
              Http::ResponseFunc([&](Http::QResponsePtr ptrResp) {
                  Q_EMIT requestRecv(ptrResp);
              }));
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
              Http::ResponseFunc([&](Http::QResponsePtr ptrResp) {
                  Q_EMIT requestRecv(ptrResp);
              }));
}

void NetWorkUtils::doRipStatus()
{
    Http::Get(Http::URL(NETWORK_ROOT_RIPSTATUS),
              Http::ResponseFunc([&](Http::QResponsePtr ptrResp) {
                  Q_EMIT requestRecv(ptrResp);
              }));
}

void NetWorkUtils::doRipVersion()
{
    Http::Get(Http::URL(NETWORK_ROOT_RIPVERSION),
              Http::ResponseFunc([&](Http::QResponsePtr ptrResp) {
                  Q_EMIT requestRecv(ptrResp);
              }));
}

void NetWorkUtils::init()
{
    // 此时已经在新线程的事件循环中
    // 启动外部程序
    m_pExeProcess = new QProcess();
    m_pExeProcess->start(ExePath);
    if (m_pExeProcess->waitForStarted(3000)) {
        qDebug() << ExePath << "启动成功！"
                 << " " << m_pExeProcess->processId();
        qDebug() << m_pExeProcess->readAllStandardOutput();
    } else {
        qWarning() << ExePath << "启动失败";
    }
}

void NetWorkUtils::onReplyFinished(Http::QResponsePtr ptrResponse)
{
    if (!ptrResponse)
        return;

    qDebug() << ptrResponse;

    QString strError = "";
    if (ptrResponse->success(strError)) {
        QJsonDocument doc = QJsonDocument::fromJson(ptrResponse->body());
        Q_EMIT requestFinished(doc);
    } else {
        Q_EMIT requestError(strError);
    }
}
