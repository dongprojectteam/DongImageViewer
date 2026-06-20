#pragma once

#include "ArchivePolicy.h"
#include "VfsItem.h"
#include "ZipReader.h"

#include <QByteArray>
#include <QString>
#include <vector>

class Vfs {
public:
    explicit Vfs(ArchivePolicy policy = {});

    [[nodiscard]] std::vector<VfsItem> openCollection(const QString& target) const;
    [[nodiscard]] QByteArray readBytes(const QString& uri) const;

private:
    ArchivePolicy policy_;
    ZipReader zipReader_;

    [[nodiscard]] std::vector<VfsItem> listDirectory(const QString& path) const;
    [[nodiscard]] std::vector<VfsItem> listArchiveUri(const QString& uri) const;
    [[nodiscard]] VfsItemType classifyPath(const QString& path) const;
};
