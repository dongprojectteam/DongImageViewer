#pragma once

#include "ArchivePolicy.h"
#include "VfsItem.h"

#include <QByteArray>
#include <QString>
#include <vector>

class ZipReader {
public:
    explicit ZipReader(ArchivePolicy policy = {});

    [[nodiscard]] std::vector<VfsItem> list(const QString& archiveUri, const QByteArray& data) const;
    [[nodiscard]] QByteArray readMember(const QByteArray& data, const QString& memberPath) const;

private:
    struct Entry {
        QString name;
        std::uint32_t compressedSize = 0;
        std::uint32_t uncompressedSize = 0;
        std::uint32_t localHeaderOffset = 0;
        std::uint16_t method = 0;
    };

    ArchivePolicy policy_;

    [[nodiscard]] std::vector<Entry> entries(const QByteArray& data) const;
    [[nodiscard]] QString normalizeMemberName(const QString& name) const;
    [[nodiscard]] QByteArray inflateRaw(const QByteArray& compressed, std::uint32_t expectedSize) const;
};
