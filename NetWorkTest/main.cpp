#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <QTimer>

#include "../Src/Utils/NetWorkUtils.h"
#include "../Src/NetWork/QHttp.h"

using namespace Http;

static const QString kBase = "http://127.0.0.1:9201";

// ================================================================
//  Main
// ================================================================
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QJsonObject jsonObj;
    jsonObj["x_resolution"] = 720;
    jsonObj["y_resolution"] = 1200;
    jsonObj["prn_path"] = "C:/Users/12257/Desktop/mt.tif";

    // 转换为 QJsonDocument 并输出紧凑格式的字符串
    QJsonDocument doc(jsonObj);
    QByteArray jsonString = doc.toJson(QJsonDocument::Compact);
    qDebug() << jsonString;

    Get(URL(kBase + "/addrip"), Parameters({ Parameter("param", jsonString) }),
        ResponseFunc([&](QResponsePtr r) {
            qDebug() << r->statusCode();
            qDebug() << r->body();
            qDebug() << r;
        }));

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, []() {
        Get(URL(kBase + "/ripstatus"), ResponseFunc([&](QResponsePtr r) {
                qDebug() << r->statusCode();
                qDebug() << r->body();
                qDebug() << r;
            }));
    });
    timer.start(3000);

    return app.exec();
}
