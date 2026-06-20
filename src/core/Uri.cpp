#include "Uri.h"

#include <QFileInfo>
#include <QUrl>

QString fileUriFromPath(const QString& path)
{
    return QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString();
}

QString archiveUri(const QString& archive, const QString& memberPath)
{
    return QStringLiteral("archive://") + QUrl::toPercentEncoding(archive, ":/!%")
        + QStringLiteral("!/") + QUrl::toPercentEncoding(memberPath, "/");
}

bool isArchiveUri(const QString& uri)
{
    return uri.startsWith(QStringLiteral("archive://"));
}

QString pathFromFileUri(const QString& uri)
{
    const QUrl url(uri);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return QFileInfo(uri).absoluteFilePath();
}

ArchiveUriParts parseArchiveUri(const QString& uri)
{
    const QString payload = uri.mid(QStringLiteral("archive://").size());
    const int marker = payload.lastIndexOf(QStringLiteral("!/"));
    if (marker < 0) {
        throw std::runtime_error("Invalid archive URI");
    }
    return {
        QUrl::fromPercentEncoding(payload.left(marker).toUtf8()),
        QUrl::fromPercentEncoding(payload.mid(marker + 2).toUtf8()),
    };
}

int archiveNestedDepth(const QString& uri)
{
    if (!isArchiveUri(uri)) {
        return 0;
    }
    int depth = 0;
    QString current = uri;
    while (isArchiveUri(current)) {
        ++depth;
        current = parseArchiveUri(current).archiveUri;
    }
    return depth;
}
