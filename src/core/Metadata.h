#pragma once

#include <QString>
#include <vector>

struct MetadataEntry {
    QString key;
    QString value;
};

struct ImageMetadata {
    std::vector<MetadataEntry> entries;
};
