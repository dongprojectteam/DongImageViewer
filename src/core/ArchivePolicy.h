#pragma once

#include <cstddef>
#include <cstdint>

struct ArchivePolicy {
    std::size_t maxEntries = 20000;
    std::uint64_t maxUncompressedSize = 2ull * 1024ull * 1024ull * 1024ull;
    std::uint64_t maxSingleFileSize = 512ull * 1024ull * 1024ull;
    std::uint64_t maxCompressionRatio = 200;
    int maxNestedDepth = 4;
};
