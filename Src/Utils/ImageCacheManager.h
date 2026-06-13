#ifndef IMAGECACHEMANAGER_H
#define IMAGECACHEMANAGER_H

#include <QImage>
#include <QMutex>
#include <QString>

/// 持久化图片缩略图缓存 — 对标 Scribus ScImageCacheManager
///
/// 线程安全单例。缓存键 = MD5(canonicalPath | mtime | fileSize)，
/// 源文件修改后自动失效。缓存格式为无损 PNG。
///
/// 用法：
///   auto &cache = ImageCacheManager::instance();
///   if (cache.hasCachedThumbnail(path))
///       thumbnail = cache.loadThumbnail(path);
///   else
///       cache.saveThumbnail(path, generateThumbnail(path));
///
class ImageCacheManager
{
public:
    static ImageCacheManager &instance();

    // ---- 核心缓存操作 (全部受 QMutex 保护) ----

    /// 是否存在有效的缓存缩略图
    bool hasCachedThumbnail(const QString &filePath) const;

    /// 从缓存加载缩略图；若不存在或无效返回空 QImage
    QImage loadThumbnail(const QString &filePath) const;

    /// 原子加载：若缓存存在则返回，否则返回空 QImage
    /// 替代 hasCachedThumbnail + loadThumbnail 的两步模式，
    /// 消除 TOCTOU 竞争（两次调用间隔中 cleanup 可能删除文件）
    QImage loadIfCached(const QString &filePath) const;

    /// 保存缩略图到缓存（先写临时文件再原子 rename）
    void saveThumbnail(const QString &filePath, const QImage &thumbnail);

    /// 清除指定文件的缓存
    void invalidate(const QString &filePath);

    /// 清空全部缓存
    void clear();

    // ---- 缓存管理 ----

    /// 缓存根目录
    QString cacheDir() const { return m_cacheDir; }

    /// 当前缓存总字节数
    qint64 cacheSizeBytes() const;

    /// 当缓存超过 maxSizeBytes 时删除最旧条目，直到低于 limit 的 80%
    /// 默认上限 500 MiB
    void cleanup(qint64 maxSizeBytes = 500LL * 1024 * 1024);

private:
    ImageCacheManager();
    ~ImageCacheManager() = default;
    Q_DISABLE_COPY(ImageCacheManager)

    /// 生成缓存键：MD5(canonicalPath + "|" + mtime + "|" + fileSize)
    QString cacheKey(const QString &filePath) const;

    /// 给定键的缓存文件完整路径
    QString cacheFilePath(const QString &key) const;

    /// 内部清理（调用方必须已持有 m_mutex）
    void cleanupLocked(qint64 maxSizeBytes);

    mutable QMutex m_mutex;
    QString m_cacheDir;
    static constexpr qint64 kDefaultMaxCacheBytes = 500LL * 1024 * 1024;
};

#endif // IMAGECACHEMANAGER_H
