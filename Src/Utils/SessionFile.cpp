#include "SessionFile.h"
#include "qatgraphicsview.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryFile>

// ======================== Tool serialization ========================

static const char *kToolNames[] = { "Select",      "Hand",     "Rect", "Ellipse", "Line",
                                    "BezierCurve", "Freehand", "Text", "Image" };

QString SessionInfo::toolToString(Tool t)
{
    int idx = static_cast<int>(t);
    if (idx >= 0 && idx < int(sizeof(kToolNames) / sizeof(kToolNames[0])))
        return QString::fromLatin1(kToolNames[idx]);
    return QStringLiteral("Select");
}

Tool SessionInfo::toolFromString(const QString &s)
{
    for (int i = 0; i < int(sizeof(kToolNames) / sizeof(kToolNames[0])); ++i) {
        if (s == QLatin1String(kToolNames[i]))
            return static_cast<Tool>(i);
    }
    return Tool::Select;
}

// ======================== SessionTabInfo <-> JSON ========================

static QJsonObject tabToJson(const SessionTabInfo &tab)
{
    QJsonObject o;
    o["modified"] = tab.modified;
    o["canvasWidthPx"] = tab.canvasWidthPx;
    o["canvasHeightPx"] = tab.canvasHeightPx;
    o["ppi"] = tab.ppi;
    o["zoomLevel"] = tab.zoomLevel;
    o["gridVisible"] = tab.gridVisible;
    return o;
}

static SessionTabInfo tabFromJson(const QJsonObject &o)
{
    SessionTabInfo tab;
    // v1 backward compat: projectPath was per-tab (now ignored — read from top-level)
    tab.modified = o.value("modified").toBool(false);
    tab.canvasWidthPx = o.value("canvasWidthPx").toDouble(0);
    tab.canvasHeightPx = o.value("canvasHeightPx").toDouble(0);
    tab.ppi = o.value("ppi").toDouble(150.0);
    tab.zoomLevel = o.value("zoomLevel").toDouble(1.0);
    tab.gridVisible = o.value("gridVisible").toBool(true);
    return tab;
}

// ======================== SessionInfo <-> JSON ========================

QJsonObject SessionInfo::toJson() const
{
    QJsonObject o;
    o["version"] = version;
    o["projectPath"] = projectPath;
    o["activeTabIndex"] = activeTabIndex;
    o["currentTool"] = toolToString(currentTool);
    o["ripEnabled"] = ripEnabled;
    o["ripXRes"] = ripXRes;
    o["ripYRes"] = ripYRes;
    o["alignHSpacing"] = alignHSpacing;
    o["alignVSpacing"] = alignVSpacing;

    QJsonArray arr;
    for (const auto &tab : tabs)
        arr.append(tabToJson(tab));
    o["tabs"] = arr;

    return o;
}

SessionInfo SessionInfo::fromJson(const QJsonObject &o)
{
    SessionInfo info;
    info.version = o.value("version").toInt(1);
    info.projectPath = o.value("projectPath").toString();
    info.activeTabIndex = o.value("activeTabIndex").toInt(0);
    info.currentTool = toolFromString(o.value("currentTool").toString());
    info.ripEnabled = o.value("ripEnabled").toBool(false);
    info.ripXRes = o.value("ripXRes").toInt(0);
    info.ripYRes = o.value("ripYRes").toInt(0);
    info.alignHSpacing = o.value("alignHSpacing").toDouble(0.0);
    info.alignVSpacing = o.value("alignVSpacing").toDouble(0.0);

    const QJsonArray arr = o.value("tabs").toArray();
    for (const auto &v : arr)
        info.tabs.append(tabFromJson(v.toObject()));

    // v1 backward compat: if tabs have per-tab projectPath, use first tab's
    if (info.projectPath.isEmpty() && !info.tabs.isEmpty()) {
        const QJsonArray arrV1 = o.value("tabs").toArray();
        if (!arrV1.isEmpty())
            info.projectPath = arrV1.first().toObject().value("projectPath").toString();
    }

    return info;
}

SessionInfo SessionInfo::fromJson(const QJsonDocument &doc)
{
    if (doc.isObject())
        return fromJson(doc.object());
    return SessionInfo();
}

// ======================== File I/O ========================

QString SessionFile::sessionFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/session.json");
}

bool SessionFile::save(const SessionInfo &info, QString *errorOut)
{
    const QString path = sessionFilePath();

    QJsonDocument doc(info.toJson());
    QByteArray json = doc.toJson(QJsonDocument::Indented);

    // Atomic write: write to temp file, then rename
    const QString tmpPath = path + QStringLiteral(".tmp");

    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = tmpFile.errorString();
        return false;
    }

    tmpFile.write(json);
    tmpFile.close();

    // Remove old file if exists (QFile::rename fails if target exists on Windows)
    QFile::remove(path);

    if (!tmpFile.rename(path)) {
        if (errorOut)
            *errorOut = tmpFile.errorString();
        QFile::remove(tmpPath);
        return false;
    }

    return true;
}

SessionInfo SessionFile::load(bool *okOut, QString *errorOut)
{
    const QString path = sessionFilePath();

    if (!QFile::exists(path)) {
        if (okOut)
            *okOut = true;
        // No session file — return empty (this is the normal first-run case)
        return SessionInfo();
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = file.errorString();
        if (okOut)
            *okOut = false;
        return SessionInfo();
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        if (errorOut)
            *errorOut = parseError.errorString();
        if (okOut)
            *okOut = false;
        return SessionInfo();
    }

    SessionInfo info = SessionInfo::fromJson(doc);
    if (okOut)
        *okOut = true;
    return info;
}

bool SessionFile::remove(QString *errorOut)
{
    const QString path = sessionFilePath();
    if (!QFile::exists(path))
        return true;

    if (!QFile::remove(path)) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to remove %1").arg(path);
        return false;
    }
    return true;
}
