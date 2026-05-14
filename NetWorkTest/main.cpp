#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include "../Src/NetWork/QHttp.h"

using namespace Http;

static const QString kBase = "http://127.0.0.1:9210";

// ================================================================
//  Main
// ================================================================
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 创建 JSON 数组并添加两个字符串
    QJsonArray listArray;
    listArray.append("m_ink_sequence0");
    listArray.append("m_ink_sequence1");

    // 创建 JSON 对象，插入 "list" 键值对
    QJsonObject jsonObj;
    jsonObj["list"] = listArray;
    // jsonObj["m_ink_sequence0"] = 1;
    // jsonObj["m_ink_sequence1"] = 2;

    // 转换为 QJsonDocument 并输出紧凑格式的字符串
    QJsonDocument doc(jsonObj);
    QByteArray jsonString = doc.toJson(QJsonDocument::Compact);
    qDebug() << jsonString;

    Get(URL(kBase + "/getParam"), Parameters({ Parameter("param", jsonString) }),
        ResponseFunc([&](QResponsePtr r) {
            qDebug() << r->statusCode();
            qDebug() << r->body();
            qDebug() << r;
        }));

    return app.exec();
}
