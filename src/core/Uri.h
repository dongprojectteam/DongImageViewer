#pragma once

#include <QString>

struct ArchiveUriParts {
    QString archiveUri;
    QString memberPath;
};

QString fileUriFromPath(const QString& path);
QString archiveUri(const QString& archiveUri, const QString& memberPath);
bool isArchiveUri(const QString& uri);
QString pathFromFileUri(const QString& uri);
ArchiveUriParts parseArchiveUri(const QString& uri);
[[nodiscard]] int archiveNestedDepth(const QString& uri);
