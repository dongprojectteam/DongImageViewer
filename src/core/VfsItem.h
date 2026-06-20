#pragma once

#include <QString>
#include <cstdint>

enum class VfsItemType {
    Directory,
    Image,
    Archive,
    Other,
};

struct VfsItem {
    QString uri;
    QString displayName;
    VfsItemType type = VfsItemType::Other;
    std::uint64_t size = 0;
    std::uint64_t compressedSize = 0;
    qint64 modifiedAt = 0;

    [[nodiscard]] bool isImage() const { return type == VfsItemType::Image; }
    [[nodiscard]] bool isArchive() const { return type == VfsItemType::Archive; }
};
