#pragma once

#include "LruCache.h"

#include <QImage>
#include <QMutex>
#include <QSize>
#include <QString>

class ThumbnailCache {
public:
    explicit ThumbnailCache(int profileSize = 96, int memoryLimit = 256);

    [[nodiscard]] QImage get(const QString& cacheKey);
    void put(const QString& cacheKey, const QImage& image);
    void rebuild();

private:
    int profileSize_;
    LruCache<QString, QImage> memoryCache_;
    QString diskRoot_;
    mutable QMutex mutex_;

    [[nodiscard]] QString diskPath(const QString& cacheKey) const;
    [[nodiscard]] QImage loadFromDisk(const QString& cacheKey) const;
    void saveToDisk(const QString& cacheKey, const QImage& image) const;
};

QString makeThumbnailCacheKey(
    const QString& uri,
    std::uint64_t sourceSize,
    qint64 modifiedAt,
    int profileSize,
    int decoderVersion = 1);
