#include "ThumbnailCache.h"

#include "AppPaths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMutexLocker>

namespace {
constexpr int kDecoderVersion = 1;
}

QString makeThumbnailCacheKey(
    const QString& uri,
    const std::uint64_t sourceSize,
    const qint64 modifiedAt,
    const int profileSize,
    const int decoderVersion)
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(uri)
        .arg(sourceSize)
        .arg(modifiedAt)
        .arg(profileSize)
        .arg(decoderVersion);
}

ThumbnailCache::ThumbnailCache(const int profileSize, const int memoryLimit)
    : profileSize_(profileSize)
    , memoryCache_(static_cast<std::size_t>(memoryLimit))
    , diskRoot_(AppPaths::thumbnailCacheRoot())
{
}

QImage ThumbnailCache::get(const QString& cacheKey)
{
    QMutexLocker lock(&mutex_);
    if (const auto cached = memoryCache_.get(cacheKey)) {
        return *cached;
    }

    QImage diskImage = loadFromDisk(cacheKey);
    if (!diskImage.isNull()) {
        memoryCache_.put(cacheKey, diskImage);
    }
    return diskImage;
}

void ThumbnailCache::put(const QString& cacheKey, const QImage& image)
{
    QMutexLocker lock(&mutex_);
    memoryCache_.put(cacheKey, image);
    saveToDisk(cacheKey, image);
}

void ThumbnailCache::rebuild()
{
    QMutexLocker lock(&mutex_);
    memoryCache_.clear();
    QDir(diskRoot_).removeRecursively();
    QDir().mkpath(diskRoot_);
}

QString ThumbnailCache::diskPath(const QString& cacheKey) const
{
    const QByteArray digest = QCryptographicHash::hash(cacheKey.toUtf8(), QCryptographicHash::Sha256).toHex();
    return diskRoot_ + "/" + QString::fromLatin1(digest) + ".png";
}

QImage ThumbnailCache::loadFromDisk(const QString& cacheKey) const
{
    const QString path = diskPath(cacheKey);
    QImage image(path);
    if (image.isNull() && QFile::exists(path)) {
        QFile::remove(path);
    }
    return image;
}

void ThumbnailCache::saveToDisk(const QString& cacheKey, const QImage& image) const
{
    image.save(diskPath(cacheKey), "PNG");
}
