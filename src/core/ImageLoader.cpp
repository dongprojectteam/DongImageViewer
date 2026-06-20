#include "ImageLoader.h"

#include <QBuffer>
#include <QColorSpace>
#include <QImageReader>
#include <QMutexLocker>
#include <QtConcurrent>
#include <stdexcept>

namespace {
ImageMetadata buildMetadata(const QImage& image, QImageReader& reader)
{
    ImageMetadata metadata;
    metadata.entries.push_back({"Format", QString::fromLatin1(reader.format()).toUpper()});
    metadata.entries.push_back({"Width", QString::number(image.width())});
    metadata.entries.push_back({"Height", QString::number(image.height())});
    metadata.entries.push_back({"Depth", QString::number(image.depth())});
    const QString colorSpace = image.colorSpace().isValid() ? image.colorSpace().description() : QStringLiteral("Unknown");
    metadata.entries.push_back({"Color Space", colorSpace});
    metadata.entries.push_back({"Auto Transform", reader.autoTransform() ? "Enabled" : "Disabled"});
    for (const QString& key : reader.textKeys()) {
        metadata.entries.push_back({key, reader.text(key)});
    }
    return metadata;
}
}

ImageLoader::ImageLoader(const Vfs& vfs)
    : vfs_(vfs)
    , imageCache_(32)
    , thumbnailCache_(96, 256)
{
}

QImage ImageLoader::load(const QString& uri)
{
    return loadWithMetadata(uri).image;
}

ImageData ImageLoader::loadWithMetadata(const QString& uri)
{
    {
        QMutexLocker lock(&mutex_);
        if (const auto cached = imageCache_.get(uri)) {
            ImageData data;
            data.image = *cached;
            data.metadata.entries.push_back({"Cache", "Memory"});
            data.metadata.entries.push_back({"Width", QString::number(data.image.width())});
            data.metadata.entries.push_back({"Height", QString::number(data.image.height())});
            return data;
        }
    }

    const QByteArray data = vfs_.readBytes(uri);
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        throw std::runtime_error(reader.errorString().toStdString());
    }

    ImageMetadata metadata = buildMetadata(image, reader);

    {
        QMutexLocker lock(&mutex_);
        imageCache_.put(uri, image);
    }
    return {image, metadata};
}

QImage ImageLoader::loadThumbnail(const VfsItem& item, const QSize& size)
{
    const QString cacheKey = makeThumbnailCacheKey(item.uri, item.size, item.modifiedAt, size.width());
    QImage cached = thumbnailCache_.get(cacheKey);
    if (!cached.isNull()) {
        return cached;
    }

    const QByteArray data = vfs_.readBytes(item.uri);
    QImage image = decodeImage(data, size);
    if (image.isNull()) {
        throw std::runtime_error("Failed to decode thumbnail");
    }
    thumbnailCache_.put(cacheKey, image);
    return image;
}

void ImageLoader::prefetch(const std::vector<QString>& uris)
{
    for (const QString& uri : uris) {
        {
            QMutexLocker lock(&mutex_);
            if (imageCache_.get(uri)) {
                continue;
            }
        }
        (void)QtConcurrent::run([this, uri]() {
            try {
                (void)load(uri);
            } catch (...) {
            }
        });
    }
}

void ImageLoader::rebuildThumbnailCache()
{
    thumbnailCache_.rebuild();
}

QImage ImageLoader::decodeImage(const QByteArray& data, const QSize& targetSize) const
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    if (targetSize.isValid()) {
        reader.setScaledSize(targetSize);
    }
    QImage image = reader.read();
    if (image.isNull()) {
        throw std::runtime_error(reader.errorString().toStdString());
    }
    return image;
}
