#pragma once

#include "Metadata.h"

#include <QImage>

struct ImageData {
    QImage image;
    ImageMetadata metadata;
};
