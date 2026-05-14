#include <QBuffer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QUrlQuery>
#include <QtNetwork/QHttpMultiPart>
#include <QtNetwork/QNetworkReply>

#include "QHttp.h"

namespace Http {

Header Headers::header(const QString &strKey)
{
    for (auto &header : m_lstValues) {
        if (header.key().compare(strKey, Qt::CaseInsensitive) == 0) {
            return header;
        }
    }
    return Header("", "");
}

void Headers::append(const Header &header)
{
    m_lstValues.append(header);
}

void Parameters::append(const Parameter &param)
{
    m_lstValues.append(param);
}

QResponse::QResponse(QObject *parent) : QObject(parent), m_nStatusCode(0) { }

QResponse::~QResponse() { }

int QResponse::statusCode()
{
    return m_nStatusCode;
}

QString QResponse::body()
{
    return m_strBody;
}

bool QResponse::success(QString &strError)
{
    if (m_nStatusCode >= 200 && m_nStatusCode < 300) {
        return true;
    }

    strError = QString("HTTP Error %1").arg(m_nStatusCode);
    return false;
}

QRequest::QRequest(QObject *parent)
    : QObject(parent)
    , m_a(false)
    , m_useFileHeader(false)
    , m_pDownloadFile(nullptr)
{ }

QRequest::~QRequest()
{
    if (m_pDownloadFile) {
        delete m_pDownloadFile;
    }
}

void QRequest::config(const Async &a)
{
    m_a = a;
}
void QRequest::config(const UseFileHeader &useFileHeader)
{
    m_useFileHeader = useFileHeader;
}
void QRequest::config(const URL &url)
{
    m_url = url;
}

void QRequest::config(const DownloadPath &downloadPath)
{
    m_downloadPath = downloadPath;
    m_pDownloadFile = new QFile(m_downloadPath.value());
}

void QRequest::config(UploadProgressFunc func)
{
    m_uploadProgressFunc = func;
}
void QRequest::config(DownloadProgressFunc func)
{
    m_downloadProgressFunc = func;
}
void QRequest::config(ResponseFunc func)
{
    m_responseFunc = func;
}
void QRequest::config(const Headers &headers)
{
    m_headers = headers;
}
void QRequest::config(const Parameters &parameters)
{
    m_parameters = parameters;
}
void QRequest::config(Multipart &multipart)
{
    m_multipart = multipart;
}
void QRequest::config(Payload &payload)
{
    m_payload = payload;
}
void QRequest::config(RawData &rawData)
{
    m_rawData = rawData;
}

QGlobal::QGlobal(QObject *parent) : QObject(parent) { }

QGlobal::~QGlobal() { }

QGlobalManagerPtr QGlobalManager::shared()
{
    static QGlobalManagerPtr s_instance =
        qmake_shared(QGlobalManager, QCoreApplication::instance());
    return s_instance;
}

QGlobalManager::QGlobalManager(QObject *parent) : QObject(parent) { }

QGlobalManager::~QGlobalManager() { }

void QGlobalManager::add(const QString &strUrlRoot, QGlobalPtr ptrGlobal)
{
    m_mapGlobals[strUrlRoot] = ptrGlobal;
}

QGlobalPtr QGlobalManager::get(const QString &strUrl)
{
    for (auto it = m_mapGlobals.begin(); it != m_mapGlobals.end(); ++it) {
        if (strUrl.startsWith(it.key())) {
            return it.value();
        }
    }
    return nullptr;
}

void QGlobalManager::updateAuthHeaders(
    const QMap<QString, QString> &mapAuthHeaders)
{
    m_mapAuthHeaders = mapAuthHeaders;
}

QMap<QString, QString> QGlobalManager::authHeaders()
{
    return m_mapAuthHeaders;
}

QHttp::QHttp(QObject *parent)
    : QObject(parent)
    , m_pManager(new QNetworkAccessManager(this))
    , m_pReply(nullptr)
    , m_multiPart(nullptr)
{ }

QHttp::~QHttp()
{
    if (m_multiPart) {
        delete m_multiPart;
    }
    for (auto file : m_lstMultiPartFiles) {
        delete file;
    }
}

void QHttp::_get()
{
    if (m_pReply) {
        m_pReply->abort();
        m_pReply->deleteLater();
        m_pReply = nullptr;
    }

    QUrl url(ptrReq->url().value());

    // 添加查询参数
    QUrlQuery query;
    for (auto &param : ptrReq->parameters().values()) {
        query.addQueryItem(param.key(), param.valueString());
    }
    url.setQuery(query);

    QNetworkRequest request(url);
    _beforeRequest(&request, HttpMethod::hmGet);
    m_pReply = m_pManager->get(request);
    _afterRequest(HttpMethod::hmGet);
}

void QHttp::_post()
{
    if (m_pReply) {
        m_pReply->abort();
        m_pReply->deleteLater();
        m_pReply = nullptr;
    }

    QUrl url(ptrReq->url().value());
    QNetworkRequest request(url);
    _beforeRequest(&request, HttpMethod::hmPost);

    if (ptrReq->multipart().values().size() > 0) {
        m_multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        for (const auto &part : ptrReq->multipart().values()) {
            QHttpPart httpPart;

            if (part.m_bIsFile) {
                QFile *file = new QFile(part.m_strData);
                if (!file->open(QIODevice::ReadOnly)) {
                    qWarning() << "Failed to open file:" << part.m_strData;
                    delete file;
                    continue;
                }

                m_lstMultiPartFiles.append(file);
                httpPart.setBodyDevice(file);

                QString disposition =
                    QString("form-data; name=\"%1\"; filename=\"%2\"")
                        .arg(part.m_strName)
                        .arg(QFileInfo(part.m_strData).fileName());
                httpPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                   disposition);
            } else {
                httpPart.setBody(part.m_strData.toUtf8());
                httpPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                   "form-data; name=\"" + part.m_strName
                                       + "\"");
            }

            if (!part.type_.isEmpty()) {
                httpPart.setHeader(QNetworkRequest::ContentTypeHeader,
                                   part.type_);
            }

            m_multiPart->append(httpPart);
        }

        m_pReply = m_pManager->post(request, m_multiPart);
        m_multiPart->setParent(m_pReply);
    } else if (!ptrReq->payload().value().isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/json");
        m_pReply =
            m_pManager->post(request, ptrReq->payload().value().toUtf8());
    } else if (!ptrReq->rawData().value().empty()) {
        m_pReply = m_pManager->post(
            request, QByteArray::fromStdString(ptrReq->rawData().value()));
    } else {
        QUrlQuery query;
        for (auto &param : ptrReq->parameters().values()) {
            query.addQueryItem(param.key(), param.valueString());
        }
        m_pReply = m_pManager->post(
            request, query.toString(QUrl::FullyEncoded).toUtf8());
    }

    _afterRequest(HttpMethod::hmPost);
}

void QHttp::_put()
{
    if (m_pReply) {
        m_pReply->abort();
        m_pReply->deleteLater();
        m_pReply = nullptr;
    }

    QUrl url(ptrReq->url().value());
    QNetworkRequest request(url);
    _beforeRequest(&request, HttpMethod::hmPut);

    if (!ptrReq->payload().value().isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/json");
        m_pReply = m_pManager->put(request, ptrReq->payload().value().toUtf8());
    } else if (!ptrReq->rawData().value().empty()) {
        m_pReply = m_pManager->put(
            request, QByteArray::fromStdString(ptrReq->rawData().value()));
    } else {
        m_pReply = m_pManager->put(request, QByteArray());
    }

    _afterRequest(HttpMethod::hmPut);
}

void QHttp::_delete()
{
    if (m_pReply) {
        m_pReply->abort();
        m_pReply->deleteLater();
        m_pReply = nullptr;
    }

    QUrl url(ptrReq->url().value());
    QNetworkRequest request(url);
    _beforeRequest(&request, HttpMethod::hmDelete);

    if (!ptrReq->payload().value().isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/json");
        auto *buffer = new QBuffer();
        buffer->setData(ptrReq->payload().value().toUtf8());
        buffer->open(QIODevice::ReadOnly);
        buffer->setParent(this);
        m_pReply = m_pManager->sendCustomRequest(request, "DELETE", buffer);
    } else {
        m_pReply = m_pManager->deleteResource(request);
    }

    _afterRequest(HttpMethod::hmDelete);
}

void QHttp::_patch()
{
    if (m_pReply) {
        m_pReply->abort();
        m_pReply->deleteLater();
        m_pReply = nullptr;
    }

    QUrl url(ptrReq->url().value());
    QNetworkRequest request(url);
    _beforeRequest(&request, HttpMethod::hmPatch);

    QByteArray body;
    if (!ptrReq->payload().value().isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/json");
        body = ptrReq->payload().value().toUtf8();
    } else if (!ptrReq->rawData().value().empty()) {
        body = QByteArray::fromStdString(ptrReq->rawData().value());
    }
    auto *buffer = new QBuffer();
    buffer->setData(body);
    buffer->open(QIODevice::ReadOnly);
    buffer->setParent(this);
    m_pReply = m_pManager->sendCustomRequest(request, "PATCH", buffer);

    _afterRequest(HttpMethod::hmPatch);
}

void QHttp::_beforeRequest(QNetworkRequest *pReq, HttpMethod method)
{
    // 添加全局头
    auto global = QGlobalManager::shared()->get(ptrReq->url().value());
    if (global) {
        for (auto &header : global->headers().values()) {
            pReq->setRawHeader(header.key().toUtf8(),
                               header.valueString().toUtf8());
        }
    }

    // 添加授权头
    auto authHeaders = QGlobalManager::shared()->authHeaders();
    for (auto it = authHeaders.begin(); it != authHeaders.end(); ++it) {
        pReq->setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    // 添加请求特定头
    for (auto &header : ptrReq->headers().values()) {
        pReq->setRawHeader(header.key().toUtf8(),
                           header.valueString().toUtf8());
    }

    // 特殊文件头处理
    if (ptrReq->useFileHeader().value()) {
        pReq->setRawHeader("X-File-Type", "");
    }
}

void QHttp::_afterRequest(HttpMethod method)
{
    // 连接信号槽
    if (ptrReq->downloadProgressFunc().value()) {
        QObject::connect(m_pReply, &QNetworkReply::downloadProgress,
                         [this](qint64 bytesReceived, qint64 bytesTotal) {
                             ptrReq->downloadProgressFunc().value()(
                                 bytesReceived, bytesTotal);
                         });
    }

    if (ptrReq->uploadProgressFunc().value()) {
        QObject::connect(m_pReply, &QNetworkReply::uploadProgress,
                         [this](qint64 bytesSent, qint64 bytesTotal) {
                             ptrReq->uploadProgressFunc().value()(bytesSent,
                                                                  bytesTotal);
                         });
    }

    // 异步处理响应
    if (ptrReq->async().value()) {
        QObject::connect(m_pReply, &QNetworkReply::finished, [this]() {
            _handleResponse();
            deleteLater();
        });
    } else {
        // 同步请求处理
        QEventLoop loop;
        QObject::connect(m_pReply, &QNetworkReply::finished,
                         [&loop]() { loop.quit(); });
        loop.exec();
        _handleResponse();
        deleteLater();
    }
}

void QHttp::_handleResponse()
{
    ptrResp = QResponsePtr(new QResponse(this));

    if (!m_pReply) {
        ptrResp->setStatusCode(0);
        ptrResp->setBody("Network reply is null");
        if (ptrReq->responseFunc().value())
            ptrReq->responseFunc().value()(ptrResp);
        return;
    }

    // 处理错误
    if (m_pReply->error() != QNetworkReply::NoError) {
        ptrResp->setStatusCode(
            m_pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                .toInt());
        ptrResp->setBody(m_pReply->errorString());
        if (ptrReq->responseFunc().value()) {
            ptrReq->responseFunc().value()(ptrResp);
        }
        return;
    }

    // 处理成功响应
    ptrResp->setStatusCode(
        m_pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());

    // 处理下载文件
    if (ptrReq->file()) {
        if (ptrReq->file()->open(QIODevice::WriteOnly)) {
            ptrReq->file()->write(m_pReply->readAll());
            ptrReq->file()->close();
        }
        ptrResp->setBody(ptrReq->file()->fileName());
    } else {
        ptrResp->setBody(QString::fromUtf8(m_pReply->readAll()));
    }

    // 处理响应头
    Headers headers;
    for (const auto &header : m_pReply->rawHeaderPairs()) {
        headers.append(Header(header.first, header.second));
    }
    ptrResp->setHeaders(headers);

    // 调用响应回调
    if (ptrReq->responseFunc().value()) {
        ptrReq->responseFunc().value()(ptrResp);
    }
}

QDebug operator<<(QDebug debug, const Part &part)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "Part("
                    << "type=" << part.type_ << ", isFile" << part.m_bIsFile
                    << ", data=" << part.m_strData << ", name" << part.m_strName
                    << ", offset" << part.m_nOffset << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const Multipart &multipart)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "Multipart("
                    << "QList=" << multipart.values() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const Parameter &parameter)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "Parameter("
                    << "key=" << parameter.key()
                    << ", value=" << parameter.value() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const Parameters &parameters)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "Parameters("
                    << "list=" << parameters.values() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const Header &header)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "Header("
                    << "key=" << header.key() << ", value=" << header.value()
                    << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const Headers &headers)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "Headers("
                    << "list=" << headers.values() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const QResponsePtr response)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "QResponse("
                    << "statusCode=" << response->statusCode()
                    << ", body=" << response->body()
                    << ", headers=" << response->headers() << ")";
    return debug;
}

} // namespace Http
