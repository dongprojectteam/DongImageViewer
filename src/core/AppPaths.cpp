#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

bool AppPaths::isPortableMode()
{
    const QString marker = QCoreApplication::applicationDirPath() + "/dongviewer.portable";
    return QFileInfo::exists(marker);
}

QString AppPaths::dataRoot()
{
    if (isPortableMode()) {
        const QString root = QCoreApplication::applicationDirPath() + "/data";
        QDir().mkpath(root);
        return root;
    }
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(root);
    return root;
}

QString AppPaths::cacheRoot()
{
    const QString root = dataRoot() + "/cache";
    QDir().mkpath(root);
    return root;
}

QString AppPaths::thumbnailCacheRoot()
{
    const QString root = cacheRoot() + "/thumbnails";
    QDir().mkpath(root);
    return root;
}

QString AppPaths::bookmarksFile()
{
    return dataRoot() + "/bookmarks.txt";
}
