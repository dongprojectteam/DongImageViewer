from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path


class ItemType(str, Enum):
    DIRECTORY = "directory"
    IMAGE = "image"
    ARCHIVE = "archive"
    OTHER = "other"


IMAGE_EXTENSIONS = {
    ".png",
    ".jpg",
    ".jpeg",
    ".webp",
    ".bmp",
    ".gif",
    ".tif",
    ".tiff",
    ".avif",
}

ARCHIVE_EXTENSIONS = {
    ".zip",
    ".cbz",
    ".tar",
    ".tgz",
    ".tar.gz",
}


@dataclass(frozen=True)
class VfsItem:
    uri: str
    display_name: str
    item_type: ItemType
    size: int = 0
    compressed_size: int = 0
    modified_at: float | None = None

    @property
    def is_image(self) -> bool:
        return self.item_type == ItemType.IMAGE

    @property
    def is_archive(self) -> bool:
        return self.item_type == ItemType.ARCHIVE


def classify_path(path: str | Path) -> ItemType:
    value = str(path).lower()
    suffix = Path(value).suffix
    if value.endswith(".tar.gz"):
        return ItemType.ARCHIVE
    if suffix in ARCHIVE_EXTENSIONS:
        return ItemType.ARCHIVE
    if suffix in IMAGE_EXTENSIONS:
        return ItemType.IMAGE
    return ItemType.OTHER
