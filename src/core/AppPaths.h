#pragma once

#include <QString>

struct AppPaths {
    [[nodiscard]] static bool isPortableMode();
    [[nodiscard]] static QString dataRoot();
    [[nodiscard]] static QString cacheRoot();
    [[nodiscard]] static QString thumbnailCacheRoot();
    [[nodiscard]] static QString bookmarksFile();
};
