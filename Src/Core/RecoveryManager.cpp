#include "RecoveryManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

// ============================================================
//  单例
// ============================================================

RecoveryManager &RecoveryManager::instance()
{
    static RecoveryManager mgr;
    return mgr;
}

RecoveryManager::RecoveryManager()
{
    m_recoveryDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                    + QStringLiteral("/recovery");
}

// ============================================================
//  路径
// ============================================================

QString RecoveryManager::recoveryDir() const
{
    return m_recoveryDir;
}

QString RecoveryManager::quitSnapshotPath() const
{
    return m_recoveryDir + QStringLiteral("/quit.atre");
}

bool RecoveryManager::ensureRecoveryDir() const
{
    return QDir().mkpath(m_recoveryDir);
}

// ============================================================
//  退出快照 (Level 3)
// ============================================================

bool RecoveryManager::saveQuitSnapshot(const QList<CanvasSaveBundle> &bundles,
                                       const QString &projectPath, const SessionInfo &session)
{
    if (!ensureRecoveryDir())
        return false;

    // 构建 ATP 格式的 XML（复用 ProjectFile 的序列化能力）
    ProjectFile::ProjectInfo info;
    info.name = QStringLiteral("Recovery");
    info.version = QStringLiteral("2.0.0");
    info.author = QStringLiteral("Recovery");
    info.description = QStringLiteral("Quit recovery snapshot");

    ProjectFile pf;
    const QString targetPath = quitSnapshotPath();
    const QString tmpPath = targetPath + QStringLiteral(".tmp");

    if (!pf.saveMulti(tmpPath, info, bundles)) {
        qWarning() << "[Recovery] Failed to write quit snapshot:" << pf.lastError();
        QFile::remove(tmpPath);
        return false;
    }

    // 原子替换
    QFile::remove(targetPath);
    if (!QFile::rename(tmpPath, targetPath)) {
        qWarning() << "[Recovery] Failed to rename quit snapshot";
        QFile::remove(tmpPath);
        return false;
    }

    // 将会话信息作为 sidecar JSON 写入（解析时快速获取元数据）
    QString sidecarPath = targetPath + QStringLiteral(".json");
    QString tmpSidecar = sidecarPath + QStringLiteral(".tmp");
    {
        QFile f(tmpSidecar);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonDocument doc(session.toJson());
            f.write(doc.toJson(QJsonDocument::Compact));
            f.close();
            QFile::remove(sidecarPath);
            QFile::rename(tmpSidecar, sidecarPath);
        }
    }

    qInfo() << "[Recovery] Quit snapshot saved:" << targetPath;
    return true;
}

bool RecoveryManager::hasQuitSnapshot() const
{
    return QFileInfo::exists(quitSnapshotPath());
}

RecoveryManager::QuitSnapshot RecoveryManager::loadQuitSnapshot() const
{
    QuitSnapshot snap;
    const QString targetPath = quitSnapshotPath();
    if (!QFileInfo::exists(targetPath))
        return snap;

    // 加载 sidecar JSON 获取元数据
    QString sidecarPath = targetPath + QStringLiteral(".json");
    if (QFileInfo::exists(sidecarPath)) {
        QFile f(sidecarPath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            snap.session = SessionInfo::fromJson(doc);
        }
    }

    // 解析 ATP 文件
    ProjectFile pf;
    ProjectFile::ProjectInfo info;
    QList<CanvasDeserialBundle> bundles;
    if (pf.parseMulti(targetPath, info, bundles)) {
        snap.canvases = bundles;
        snap.projectPath = snap.session.projectPath;
        snap.timestamp = QFileInfo(targetPath).lastModified();
        snap.isValid = true;
    } else {
        qWarning() << "[Recovery] Failed to parse quit snapshot:" << pf.lastError();
    }

    return snap;
}

void RecoveryManager::discardQuitSnapshot()
{
    const QString targetPath = quitSnapshotPath();
    QFile::remove(targetPath);
    QFile::remove(targetPath + QStringLiteral(".json"));
    qInfo() << "[Recovery] Quit snapshot discarded";
}

// ============================================================
//  定时自动保存 (Level 2)
// ============================================================

bool RecoveryManager::saveAutoSnapshot(const QList<CanvasSaveBundle> &bundles,
                                       const QString &projectHash)
{
    if (!ensureRecoveryDir())
        return false;

    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString fileName = QStringLiteral("autosave_%1_%2.atre").arg(projectHash, timestamp);
    const QString targetPath = m_recoveryDir + QStringLiteral("/") + fileName;
    const QString tmpPath = targetPath + QStringLiteral(".tmp");

    ProjectFile::ProjectInfo info;
    info.name = QStringLiteral("AutoSave");
    info.version = QStringLiteral("2.0.0");
    info.author = QStringLiteral("AutoSave");

    ProjectFile pf;
    if (!pf.saveMulti(tmpPath, info, bundles)) {
        qWarning() << "[Recovery] Auto-save failed:" << pf.lastError();
        QFile::remove(tmpPath);
        emit autoSaveFailed(pf.lastError());
        return false;
    }

    QFile::remove(targetPath);
    if (!QFile::rename(tmpPath, targetPath)) {
        QFile::remove(tmpPath);
        emit autoSaveFailed(QStringLiteral("rename failed"));
        return false;
    }

    emit autoSaveCompleted();
    return true;
}

QList<RecoveryManager::AutoSnapshotEntry> RecoveryManager::listAutoSnapshots() const
{
    QList<AutoSnapshotEntry> entries;
    QDir dir(m_recoveryDir);
    const auto files = dir.entryList({ QStringLiteral("autosave_*.atre") }, QDir::Files,
                                     QDir::Time | QDir::Reversed);

    for (const auto &f : files) {
        AutoSnapshotEntry entry;
        entry.filePath = dir.filePath(f);
        entry.timestamp = QFileInfo(entry.filePath).lastModified();
        entries.append(entry);
    }
    return entries;
}

void RecoveryManager::cleanAutoSnapshots(int keepHours)
{
    QDir dir(m_recoveryDir);
    const auto files = dir.entryList({ QStringLiteral("autosave_*.atre") }, QDir::Files);
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-keepHours * 3600);

    for (const auto &f : files) {
        QFileInfo fi(dir.filePath(f));
        if (fi.lastModified() < cutoff)
            QFile::remove(fi.filePath());
    }
}
