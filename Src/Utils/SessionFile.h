#ifndef SESSIONFILE_H
#define SESSIONFILE_H

#include <QJsonObject>
#include <QList>
#include <QString>

// Forward-declared in qatgraphicsview.h
enum class Tool;

// Single tab entry in the session JSON (v2: projectPath moved to project level)
struct SessionTabInfo
{
    bool modified = false;
    double canvasWidthPx = 0;
    double canvasHeightPx = 0;
    double ppi = 150.0;
    double zoomLevel = 1.0;
    bool gridVisible = true;
};

// Full session state (v2: one project path for all tabs)
struct SessionInfo
{
    int version = 2;
    QString projectPath; // 工程文件路径（一个工程包含多个画布），空表示未保存
    int activeTabIndex = 0;
    Tool currentTool; // default Tool::Select, serialized as string
    bool ripEnabled = false;
    int ripXRes = 0;
    int ripYRes = 0;
    double alignHSpacing = 0.0;
    double alignVSpacing = 0.0;
    QList<SessionTabInfo> tabs;

    QJsonObject toJson() const;
    static SessionInfo fromJson(const QJsonObject &obj);
    static SessionInfo fromJson(const QJsonDocument &doc);

    static QString toolToString(Tool t);
    static Tool toolFromString(const QString &s);
};

// File-level read/write operations
namespace SessionFile {

// Canonical path: AppConfigLocation/session.json
QString sessionFilePath();

// Atomic write (temp file + rename)
bool save(const SessionInfo &info, QString *errorOut = nullptr);

// Read and parse. Returns empty SessionInfo (tabs empty) if file not found.
SessionInfo load(bool *okOut = nullptr, QString *errorOut = nullptr);

// Delete the session file
bool remove(QString *errorOut = nullptr);

} // namespace SessionFile

#endif // SESSIONFILE_H
