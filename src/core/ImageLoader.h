#pragma once

#include "ImageData.h"
#include "LruCache.h"
#include "ThumbnailCache.h"
#include "Vfs.h"
#include "VfsItem.h"

#include <QImage>
#include <QMutex>
#include <QSize>
#include <QString>
#include <vector>

class ImageLoader {
public:
    explicit ImageLoader(const Vfs& vfs);

    [[nodiscard]] QImage load(const QString& uri);
    [[nodiscard]] ImageData loadWithMetadata(const QString& uri);
    [[nodiscard]] QImage loadThumbnail(const VfsItem& item, const QSize& size = {96, 96});
    void prefetch(const std::vector<QString>& uris);
    void rebuildThumbnailCache();

private:
    const Vfs& vfs_;
    LruCache<QString, QImage> imageCache_;
    ThumbnailCache thumbnailCache_;
    mutable QMutex mutex_;

    [[nodiscard]] QImage decodeImage(const QByteArray& data, const QSize& targetSize = {}) const;
};
