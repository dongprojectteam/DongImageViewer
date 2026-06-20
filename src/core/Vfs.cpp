#include "Vfs.h"

#include "Uri.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <stdexcept>

namespace {
void ensureNestedDepthAllowed(const QString& uri, const ArchivePolicy& policy)
{
    if (archiveNestedDepth(uri) > policy.maxNestedDepth) {
        throw std::runtime_error("Archive nesting depth limit exceeded");
    }
}
}

Vfs::Vfs(ArchivePolicy policy)
    : policy_(policy)
    , zipReader_(policy)
{
}

std::vector<VfsItem> Vfs::openCollection(const QString& target) const
{
    QString uri = target;
    if (!uri.startsWith("file://") && !uri.startsWith("archive://")) {
        uri = fileUriFromPath(target);
    }
    if (isArchiveUri(uri)) {
        return listArchiveUri(uri);
    }

    const QString path = pathFromFileUri(uri);
    const QFileInfo info(path);
    if (info.isDir()) {
        return listDirectory(path);
    }

    const VfsItemType type = classifyPath(path);
    if (type == VfsItemType::Archive) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Failed to open archive");
        }
        return zipReader_.list(fileUriFromPath(path), file.readAll());
    }
    if (type == VfsItemType::Image) {
        return listDirectory(info.absolutePath());
    }
    return {};
}

QByteArray Vfs::readBytes(const QString& uri) const
{
    ensureNestedDepthAllowed(uri, policy_);
    if (isArchiveUri(uri)) {
        const ArchiveUriParts parsed = parseArchiveUri(uri);
        if (isArchiveUri(parsed.archiveUri)) {
            const QByteArray nestedArchive = readBytes(parsed.archiveUri);
            return zipReader_.readMember(nestedArchive, parsed.memberPath);
        }
        QFile archive(pathFromFileUri(parsed.archiveUri));
        if (!archive.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Failed to open archive");
        }
        return zipReader_.readMember(archive.readAll(), parsed.memberPath);
    }

    QFile file(pathFromFileUri(uri));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open file");
    }
    return file.readAll();
}

std::vector<VfsItem> Vfs::listDirectory(const QString& path) const
{
    QDir dir(path);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    std::vector<VfsItem> result;
    for (const QFileInfo& entry : entries) {
        VfsItemType type = entry.isDir() ? VfsItemType::Directory : classifyPath(entry.absoluteFilePath());
        if (type == VfsItemType::Other) {
            continue;
        }
        result.push_back({
            fileUriFromPath(entry.absoluteFilePath()),
            entry.fileName(),
            type,
            static_cast<std::uint64_t>(entry.size()),
            static_cast<std::uint64_t>(entry.size()),
            entry.lastModified().toMSecsSinceEpoch(),
        });
    }
    return result;
}

std::vector<VfsItem> Vfs::listArchiveUri(const QString& uri) const
{
    ensureNestedDepthAllowed(uri, policy_);
    const QByteArray archiveData = readBytes(uri);
    return zipReader_.list(uri, archiveData);
}

VfsItemType Vfs::classifyPath(const QString& path) const
{
    const QString lower = path.toLower();
    if (lower.endsWith(".zip") || lower.endsWith(".cbz")) {
        return VfsItemType::Archive;
    }
    static const QStringList imageExts = {
        ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif", ".tif", ".tiff", ".avif"
    };
    for (const QString& ext : imageExts) {
        if (lower.endsWith(ext)) {
            return VfsItemType::Image;
        }
    }
    return VfsItemType::Other;
}
