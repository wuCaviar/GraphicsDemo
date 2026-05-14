#ifndef QHTTP_H
#define QHTTP_H

#include <QFile>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>
#include <functional>

#include "../CommonDefs.h"
#include "../QCommonDefs.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QHttpMultiPart;

namespace Http {

class URL;
class DownloadPath;

class Header;
class Headers;
class Parameter;
class Parameters;

class DownloadProgressFunc;
class UploadProgressFunc;
class ResponseFunc;

enum class HttpMethod
{
    hmGet,
    hmPost,
    hmPut,
    hmDelete,
    hmPatch
};

#define ClassWrapper(cls, type)                    \
    CLASS_TYPEDEFS(cls)                            \
    class cls                                      \
    {                                              \
    public:                                        \
        cls() = default;                           \
        cls(const type &value) : m_value(value){}; \
        ~cls(){};                                  \
        type value() const { return m_value; }     \
                                                   \
    private:                                       \
        type m_value;                              \
    };

#define FieldWrapper(cls)                                     \
    CLASS_TYPEDEFS(cls)                                       \
    class cls                                                 \
    {                                                         \
    public:                                                   \
        template<typename T>                                  \
        cls(const QString &strKey, T &&t)                     \
        {                                                     \
            m_strKey = strKey;                                \
            m_value = QVariant(t);                            \
        }                                                     \
        ~cls(){};                                             \
        QString key() const { return m_strKey; };             \
        QString valueString() { return m_value.toString(); }; \
        QVariant value() const { return m_value; };           \
                                                              \
    private:                                                  \
        QString m_strKey;                                     \
        QVariant m_value;                                     \
    };

#define LIST_INITIALIZER_SUPPORT(clss, cls)           \
public:                                               \
    clss(const std::initializer_list<cls> &values)    \
    {                                                 \
        for (const auto &value : values) {            \
            m_lstValues.append(value);                \
        }                                             \
    }                                                 \
    QList<cls> values() const { return m_lstValues; } \
                                                      \
    void clear() { m_lstValues.clear(); }             \
                                                      \
private:                                              \
    QList<cls> m_lstValues;

ClassWrapper(Async, bool);

ClassWrapper(UseFileHeader, bool);

ClassWrapper(URL, QString);

ClassWrapper(DownloadPath, QString);

ClassWrapper(Payload, QString);

ClassWrapper(RawData, std::string);

ClassWrapper(DownloadProgressFunc, std::function<void(qint64, qint64)>);

ClassWrapper(UploadProgressFunc, std::function<void(qint64, qint64)>);

FieldWrapper(Header);

FieldWrapper(Parameter);

class Part
{
public:
    Part() : m_bIsFile(false), m_nOffset(0) { }

    Part(QString strPath, QString strName = "", int nOffset = 0)
        : m_bIsFile{ true }
        , m_nOffset(nOffset)
        , m_strData(strPath)
        , m_strName(strName){};

public:
    QString type_;
    bool m_bIsFile;
    QString m_strData;
    QString m_strName;

    int m_nOffset;
};

class Multipart
{
public:
    Multipart() { }

    ~Multipart() { }

    LIST_INITIALIZER_SUPPORT(Multipart, Part);
};

class Headers
{
public:
    Headers() { }

    ~Headers() { }

    LIST_INITIALIZER_SUPPORT(Headers, Header);

public:
    Header header(const QString &strKey);

    void append(const Header &header);
};

class Parameters
{
public:
    Parameters() { }

    ~Parameters() { }

    LIST_INITIALIZER_SUPPORT(Parameters, Parameter);

public:
    void append(const Parameter &param);
};

Q_CLASS_TYPEDEFS(QResponse)

class QResponse : public QObject
{
    Q_OBJECT

public:
    QResponse(QObject *parent = nullptr);

    ~QResponse();

    Headers &headers() { return m_headers; };
    void setHeaders(const Headers &headers) { m_headers = headers; }

    int statusCode();

    void setStatusCode(int nStatusCode) { m_nStatusCode = nStatusCode; }

    QString body();

    void setBody(const QString &strBody) { m_strBody = strBody; }

    bool success(QString &strError);

private:
    Headers m_headers;
    int m_nStatusCode;
    QString m_strBody;
};

ClassWrapper(ResponseFunc, std::function<void(QResponsePtr ptrResp)>);

Q_CLASS_TYPEDEFS(QRequest)

class QRequest : public QObject
{
    Q_OBJECT

public:
    QRequest(QObject *parent = nullptr);

    ~QRequest();

    void config(const Async &a);

    void config(const UseFileHeader &useFileHeader);

    void config(const URL &url);

    void config(const DownloadPath &downloadPath);

    void config(UploadProgressFunc func);

    void config(DownloadProgressFunc func);

    void config(ResponseFunc func);

    void config(const Headers &headers);

    void config(const Parameters &parameters);

    void config(Multipart &multipart);

    void config(Payload &payload);

    void config(RawData &rawData);

public:
    Async async() { return m_a; }
    UseFileHeader useFileHeader() { return m_useFileHeader; }
    URL url() { return m_url; }
    UploadProgressFunc uploadProgressFunc() { return m_uploadProgressFunc; }
    DownloadProgressFunc downloadProgressFunc()
    {
        return m_downloadProgressFunc;
    }
    ResponseFunc responseFunc() { return m_responseFunc; }
    Headers headers() { return m_headers; }
    Parameters parameters() { return m_parameters; }

    QFile *file() { return m_pDownloadFile; }

    Multipart multipart() { return m_multipart; }
    Payload payload() { return m_payload; }
    RawData rawData() { return m_rawData; }

private:
    Async m_a;
    UseFileHeader m_useFileHeader;
    URL m_url;
    DownloadPath m_downloadPath;
    UploadProgressFunc m_uploadProgressFunc;
    DownloadProgressFunc m_downloadProgressFunc;
    ResponseFunc m_responseFunc;
    Headers m_headers;
    Parameters m_parameters;
    Multipart m_multipart;
    Payload m_payload;
    RawData m_rawData;

    QFile *m_pDownloadFile;
};

Q_CLASS_TYPEDEFS(QGlobal)

class QGlobal : public QObject
{
    Q_OBJECT

public:
    QGlobal(QObject *parent = nullptr);

    ~QGlobal();

    template<typename T>
    void appendHeader(const QString &strkey, T &&t)
    {
        m_headers.append(Header(strkey, t));
    }

    Headers headers() { return m_headers; };

private:
    Headers m_headers;
};

Q_CLASS_TYPEDEFS(QGlobalManager)

class QGlobalManager : public QObject
{
    Q_OBJECT

public:
    static QGlobalManagerPtr shared();

public:
    QGlobalManager(QObject *parent = nullptr);

    ~QGlobalManager();

    void add(const QString &strUrlRoot, QGlobalPtr ptrGlobal);

    QGlobalPtr get(const QString &strUrl);

    void updateAuthHeaders(const QMap<QString, QString> &mapAuthHeaders);

    QMap<QString, QString> authHeaders();

private:
    QMap<QString, QGlobalPtr> m_mapGlobals;
    QMap<QString, QString> m_mapAuthHeaders;
};

Q_CLASS_TYPEDEFS(QHttp)

class QHttp : public QObject
{
    Q_OBJECT

public:
    QHttp(QObject *parent = nullptr);

    ~QHttp();

    template<typename... Ts>
    void get(Ts &&...ts)
    {
        ptrReq = qmake_shared(QRequest, this);
        _beforeRequest(ts...);
        _get();
    };

    template<typename... Ts>
    void post(Ts &&...ts)
    {
        ptrReq = qmake_shared(QRequest, this);
        _beforeRequest(ts...);
        _post();
    };

    template<typename... Ts>
    void put(Ts &&...ts)
    {
        ptrReq = qmake_shared(QRequest, this);
        _beforeRequest(ts...);
        _put();
    };

    template<typename... Ts>
    void deleteReq(Ts &&...ts)
    {
        ptrReq = qmake_shared(QRequest, this);
        _beforeRequest(ts...);
        _delete();
    };

    template<typename... Ts>
    void patch(Ts &&...ts)
    {
        ptrReq = qmake_shared(QRequest, this);
        _beforeRequest(ts...);
        _patch();
    };

protected:
    void _get();

    void _post();

    void _put();

    void _delete();

    void _patch();

    void _beforeRequest(QNetworkRequest *pReq, HttpMethod method);

    void _afterRequest(HttpMethod method);

    void _handleResponse();

protected:
    template<typename T>
    void _beforeRequest(T &&t)
    {
        ptrReq->config(t);
    };

    template<typename T, typename... Ts>
    void _beforeRequest(T &&t, Ts &&...ts)
    {
        _beforeRequest(t);
        _beforeRequest(ts...);
    };

protected:
    QRequestPtr ptrReq;
    QResponsePtr ptrResp;

    QNetworkAccessManager *m_pManager;
    QNetworkReply *m_pReply;
    QHttpMultiPart *m_multiPart;

    QByteArray m_postData;
    QByteArray m_putData;

    QList<QFile *> m_lstMultiPartFiles;
};

template<typename... Ts>
QHttpPtr Get(Ts &&...ts)
{
    auto pSession = qmake_shared(QHttp);
    pSession->get(ts...);
    return pSession;
}

template<typename... Ts>
QHttpPtr Post(Ts &&...ts)
{
    auto pSession = qmake_shared(QHttp);
    pSession->post(ts...);
    return pSession;
}

template<typename... Ts>
QHttpPtr Put(Ts &&...ts)
{
    auto pSession = qmake_shared(QHttp);
    pSession->put(ts...);
    return pSession;
}

template<typename... Ts>
QHttpPtr Delete(Ts &&...ts)
{
    auto pSession = qmake_shared(QHttp);
    pSession->deleteReq(ts...);
    return pSession;
}

template<typename... Ts>
QHttpPtr Patch(Ts &&...ts)
{
    auto pSession = qmake_shared(QHttp);
    pSession->patch(ts...);
    return pSession;
}

#if !defined(QT_NO_DEBUG_STREAM)

QDebug operator<<(QDebug debug, const Part &part);
QDebug operator<<(QDebug debug, const Multipart &multipart);

QDebug operator<<(QDebug debug, const Parameter &parameter);
QDebug operator<<(QDebug debug, const Parameters &parameters);

QDebug operator<<(QDebug debug, const Header &header);
QDebug operator<<(QDebug debug, const Headers &headers);

QDebug operator<<(QDebug debug, const QResponsePtr response);

#endif

} // namespace Http

#endif // QHTTP_H
