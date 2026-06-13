#include "ImageCacheManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QMutexLocker>
#include <QStandardPaths>

#include "atDefine.h"

// ============================================================================
//  单例
// ============================================================================

ImageCacheManager &ImageCacheManager::instance()
{
    static ImageCacheManager mgr;
    return mgr;
}

// ============================================================================
//  构造：确定缓存目录
// ============================================================================

ImageCacheManager::ImageCacheManager()
{
    // 优先使用 QStandardPaths::CacheLocation
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (m_cacheDir.isEmpty()) {
        // 回退到 <appDir>/cache
        m_cacheDir = QCoreApplication::applicationDirPath() + QStringLiteral("/cache");
    }
    m_cacheDir += QStringLiteral("/img/");

    // 确保目录存在
    if (!QDir().mkpath(m_cacheDir)) {
        qWarning() << "[ImageCache] Failed to create cache dir:" << m_cacheDir;
    }

    // 清理残留的 .tmp 文件（上次崩溃遗留）
    QDir dir(m_cacheDir);
    const auto tmpFiles = dir.entryList({QStringLiteral("*.tmp")}, QDir::Files);
    for (const auto &f : tmpFiles)
        dir.remove(f);

    qInfo() << "[ImageCache] Cache dir:" << m_cacheDir;
}

// ============================================================================
//  缓存键 = MD5(canonicalPath | mtime | fileSize)
//  源文件修改后自动失效：mtime 或 size 变化 → key 变化 → 旧缓存孤儿
// ============================================================================

QString ImageCacheManager::cacheKey(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (!fi.exists())
        return {};

    QString canonical = fi.canonicalFilePath();
    QString payload = canonical
                      + QStringLiteral("|")
                      + QString::number(fi.lastModified().toMSecsSinceEpoch())
                      + QStringLiteral("|")
                      + QString::number(fi.size());

    return QString::fromLatin1(
        QCryptographicHash::hash(payload.toUtf8(), QCryptographicHash::Md5).toHex());
}

QString ImageCacheManager::cacheFilePath(const QString &key) const
{
    return m_cacheDir + key + QStringLiteral(".png");
}

// ============================================================================
//  核心操作
// ============================================================================

bool ImageCacheManager::hasCachedThumbnail(const QString &filePath) const
{
    QMutexLocker lock(&m_mutex);

    QString key = cacheKey(filePath);
    if (key.isEmpty())
        return false;

    QFileInfo fi(cacheFilePath(key));
    return fi.exists() && fi.size() > 0;
}

QImage ImageCacheManager::loadThumbnail(const QString &filePath) const
{
    QMutexLocker lock(&m_mutex);

    QString key = cacheKey(filePath);
    if (key.isEmpty())
        return {};

    QString path = cacheFilePath(key);
    if (!QFileInfo::exists(path))
        return {};

    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read();
}

QImage ImageCacheManager::loadIfCached(const QString &filePath) const
{
    QMutexLocker lock(&m_mutex);

    QString key = cacheKey(filePath);
    if (key.isEmpty())
        return {};

    QString path = cacheFilePath(key);
    if (!QFileInfo::exists(path)) {
        atDebug() << "[ImageCache] Miss:" << filePath;
        return {};
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.size().isEmpty()) {
        qWarning() << "[ImageCache] loadIfCached: read failed for" << path;
        return {};
    }

    atDebug() << "[ImageCache] Hit:" << path;
    return img;
}

void ImageCacheManager::saveThumbnail(const QString &filePath, const QImage &thumbnail)
{
    if (thumbnail.isNull()) {
        qWarning() << "[ImageCache] saveThumbnail: thumbnail is null for" << filePath;
        return;
    }

    QMutexLocker lock(&m_mutex);

    QString key = cacheKey(filePath);
    if (key.isEmpty()) {
        qWarning() << "[ImageCache] saveThumbnail: empty cache key for" << filePath;
        return;
    }

    if (!QDir().mkpath(m_cacheDir)) {
        qWarning() << "[ImageCache] saveThumbnail: mkpath failed for" << m_cacheDir;
        return;
    }

    QString targetPath = cacheFilePath(key);
    QString tmpPath = targetPath + QStringLiteral(".tmp");

    // 原子写入：先写临时文件，再 rename
    // QImageWriter 必须在 rename 前析构（Windows 上析构才释放文件句柄）
    {
        QImageWriter writer(tmpPath, "png");
        writer.setCompression(1); // 快速压缩
        if (!writer.write(thumbnail)) {
            qWarning() << "[ImageCache] saveThumbnail: write failed for" << tmpPath
                       << "error:" << writer.errorString();
            QFile::remove(tmpPath);
            return;
        }
    } // writer 析构 → 关闭文件句柄

    // 原子替换
    QFile::remove(targetPath);
    if (!QFile::rename(tmpPath, targetPath)) {
        qWarning() << "[ImageCache] saveThumbnail: rename failed"
                   << tmpPath << "->" << targetPath;
        QFile::remove(tmpPath);
        return;
    }

    atDebug() << "[ImageCache] Saved:" << targetPath;

    // 缓存超限时自动清理（mutex 已持有，直接调用内部方法）
    cleanupLocked(kDefaultMaxCacheBytes);
}

void ImageCacheManager::invalidate(const QString &filePath)
{
    QMutexLocker lock(&m_mutex);

    QString key = cacheKey(filePath);
    if (!key.isEmpty())
        QFile::remove(cacheFilePath(key));
}

void ImageCacheManager::clear()
{
    QMutexLocker lock(&m_mutex);

    QDir dir(m_cacheDir);
    const auto entries = dir.entryList({QStringLiteral("*.png")}, QDir::Files);
    for (const auto &entry : entries)
        dir.remove(entry);
}

qint64 ImageCacheManager::cacheSizeBytes() const
{
    QMutexLocker lock(&m_mutex);

    qint64 total = 0;
    QDir dir(m_cacheDir);
    const auto entries = dir.entryList({QStringLiteral("*.png")}, QDir::Files);
    for (const auto &entry : entries) {
        QFileInfo fi(dir.filePath(entry));
        total += fi.size();
    }
    return total;
}

void ImageCacheManager::cleanup(qint64 maxSizeBytes)
{
    QMutexLocker lock(&m_mutex);
    cleanupLocked(maxSizeBytes);
}

void ImageCacheManager::cleanupLocked(qint64 maxSizeBytes)
{
    // 调用方必须已持有 m_mutex
    QDir dir(m_cacheDir);
    const auto entries = dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files);

    qint64 total = 0;
    for (const auto &fi : entries)
        total += fi.size();

    if (total <= maxSizeBytes)
        return;

    // 按修改时间升序排列，删除最旧直到低于上限的 80%
    qint64 target = maxSizeBytes * 4 / 5;
    auto sorted = entries;
    std::sort(sorted.begin(), sorted.end(),
              [](const QFileInfo &a, const QFileInfo &b) {
                  return a.lastModified() < b.lastModified();
              });

    for (const auto &fi : sorted) {
        if (total <= target)
            break;
        qint64 sz = fi.size();
        QFile::remove(fi.filePath());
        total -= sz;
    }
}
