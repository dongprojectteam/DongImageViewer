#include "ZipReader.h"

#include "Uri.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <stdexcept>
#include <zlib.h>

namespace {
std::uint16_t read16(const QByteArray& data, int offset)
{
    const auto* p = reinterpret_cast<const unsigned char*>(data.constData() + offset);
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t read32(const QByteArray& data, int offset)
{
    const auto* p = reinterpret_cast<const unsigned char*>(data.constData() + offset);
    return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

VfsItemType classifyName(const QString& name)
{
    const QString lower = name.toLower();
    if (lower.endsWith(".zip") || lower.endsWith(".cbz")) {
        return VfsItemType::Archive;
    }
    static const QStringList imageExts = {
        ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif", ".tif", ".tiff", ".avif"
    };
    for (const QString& ext : imageExts) {
        if (lower.endsWith(ext)) {
            return VfsItemType::Image;
        }
    }
    return VfsItemType::Other;
}
}

ZipReader::ZipReader(ArchivePolicy policy)
    : policy_(policy)
{
}

std::vector<VfsItem> ZipReader::list(const QString& archiveRefUri, const QByteArray& data) const
{
    std::vector<VfsItem> items;
    std::uint64_t totalSize = 0;
    for (const Entry& entry : entries(data)) {
        totalSize += entry.uncompressedSize;
        if (totalSize > policy_.maxUncompressedSize) {
            throw std::runtime_error("Archive uncompressed size limit exceeded");
        }

        const VfsItemType type = classifyName(entry.name);
        if (type == VfsItemType::Other) {
            continue;
        }
        items.push_back({
            archiveUri(archiveRefUri, entry.name),
            entry.name,
            type,
            entry.uncompressedSize,
            entry.compressedSize,
            0,
        });
    }
    return items;
}

QByteArray ZipReader::readMember(const QByteArray& data, const QString& memberPath) const
{
    for (const Entry& entry : entries(data)) {
        if (entry.name != memberPath) {
            continue;
        }
        const int header = static_cast<int>(entry.localHeaderOffset);
        if (header + 30 > data.size() || read32(data, header) != 0x04034b50) {
            throw std::runtime_error("Invalid ZIP local header");
        }
        const int nameLen = read16(data, header + 26);
        const int extraLen = read16(data, header + 28);
        const int payloadOffset = header + 30 + nameLen + extraLen;
        if (payloadOffset < 0 || payloadOffset + static_cast<int>(entry.compressedSize) > data.size()) {
            throw std::runtime_error("Invalid ZIP payload range");
        }
        const QByteArray compressed = data.mid(payloadOffset, static_cast<int>(entry.compressedSize));
        if (entry.method == 0) {
            return compressed;
        }
        if (entry.method == 8) {
            return inflateRaw(compressed, entry.uncompressedSize);
        }
        throw std::runtime_error("Unsupported ZIP compression method");
    }
    throw std::runtime_error("Archive member not found");
}

std::vector<ZipReader::Entry> ZipReader::entries(const QByteArray& data) const
{
    if (data.size() < 22) {
        throw std::runtime_error("ZIP file is too small");
    }

    int eocd = -1;
    const int minOffset = std::max(0, static_cast<int>(data.size()) - 22 - 65535);
    for (int i = data.size() - 22; i >= minOffset; --i) {
        if (read32(data, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        throw std::runtime_error("ZIP central directory not found");
    }

    const auto entryCount = read16(data, eocd + 10);
    const auto directorySize = read32(data, eocd + 12);
    const auto directoryOffset = read32(data, eocd + 16);
    if (entryCount > policy_.maxEntries) {
        throw std::runtime_error("Archive has too many entries");
    }
    if (directoryOffset + directorySize > static_cast<std::uint32_t>(data.size())) {
        throw std::runtime_error("Invalid ZIP central directory range");
    }

    std::vector<Entry> result;
    int offset = static_cast<int>(directoryOffset);
    for (std::uint16_t i = 0; i < entryCount; ++i) {
        if (offset + 46 > data.size() || read32(data, offset) != 0x02014b50) {
            throw std::runtime_error("Invalid ZIP central directory entry");
        }

        const auto method = read16(data, offset + 10);
        const auto compressedSize = read32(data, offset + 20);
        const auto uncompressedSize = read32(data, offset + 24);
        const auto nameLen = read16(data, offset + 28);
        const auto extraLen = read16(data, offset + 30);
        const auto commentLen = read16(data, offset + 32);
        const auto localHeaderOffset = read32(data, offset + 42);
        const int nameOffset = offset + 46;
        if (nameOffset + nameLen > data.size()) {
            throw std::runtime_error("Invalid ZIP entry name range");
        }

        QString name = normalizeMemberName(QString::fromUtf8(data.mid(nameOffset, nameLen)));
        if (!name.endsWith('/')) {
            if (uncompressedSize > policy_.maxSingleFileSize) {
                throw std::runtime_error("Archive member is too large");
            }
            if (compressedSize > 0 && uncompressedSize / compressedSize > policy_.maxCompressionRatio) {
                throw std::runtime_error("Suspicious ZIP compression ratio");
            }
            result.push_back({name, compressedSize, uncompressedSize, localHeaderOffset, method});
        }
        offset += 46 + nameLen + extraLen + commentLen;
    }
    return result;
}

QString ZipReader::normalizeMemberName(const QString& name) const
{
    QString normalized = name;
    normalized.replace('\\', '/');
    if (normalized.isEmpty() || normalized.startsWith('/')) {
        throw std::runtime_error("Unsafe archive path");
    }
    const QStringList parts = normalized.split('/', Qt::SkipEmptyParts);
    if (parts.size() > 256 || parts.join('/') != normalized.trimmed().remove(QRegularExpression("^/+"))) {
        throw std::runtime_error("Unsafe archive path");
    }
    for (const QString& part : parts) {
        if (part == "." || part == "..") {
            throw std::runtime_error("Unsafe archive path");
        }
    }
    return normalized;
}

QByteArray ZipReader::inflateRaw(const QByteArray& compressed, std::uint32_t expectedSize) const
{
    QByteArray output;
    output.resize(static_cast<int>(expectedSize));

    z_stream stream {};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib");
    }
    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (result != Z_STREAM_END) {
        throw std::runtime_error("Failed to inflate ZIP member");
    }
    output.resize(static_cast<int>(stream.total_out));
    return output;
}
