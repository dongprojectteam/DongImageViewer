from __future__ import annotations

from pathlib import Path

from .archive import ArchiveReader, to_vfs_items
from .models import ItemType, VfsItem, classify_path
from .security import ArchivePolicy
from .uri import file_uri, is_archive_uri, parse_archive_uri, parse_file_uri


class VirtualFileSystem:
    def __init__(self, policy: ArchivePolicy | None = None):
        self.archive_reader = ArchiveReader(policy)

    def open_collection(self, target: str | Path) -> list[VfsItem]:
        uri = str(target)
        if not uri.startswith("file://") and not uri.startswith("archive://"):
            path = Path(target)
            uri = file_uri(path)
        if is_archive_uri(uri):
            return self._list_archive_uri(uri)

        path = parse_file_uri(uri)
        if path.is_dir():
            return self._list_directory(path)
        item_type = classify_path(path)
        if item_type == ItemType.ARCHIVE:
            entries = self.archive_reader.list(str(path))
            return to_vfs_items(file_uri(path), entries)
        if item_type == ItemType.IMAGE:
            return self._list_directory(path.parent, selected=path)
        return []

    def read_bytes(self, uri: str) -> bytes:
        if is_archive_uri(uri):
            parsed = parse_archive_uri(uri)
            if is_archive_uri(parsed.archive_uri):
                parent_bytes = self.read_bytes(parsed.archive_uri)
                return self.archive_reader.read_member(parsed.archive_uri, parsed.member_path, parent_bytes)
            archive_path = parse_file_uri(parsed.archive_uri)
            return self.archive_reader.read_member(str(archive_path), parsed.member_path)

        path = parse_file_uri(uri)
        return path.read_bytes()

    def _list_directory(self, path: Path, selected: Path | None = None) -> list[VfsItem]:
        items: list[VfsItem] = []
        for child in sorted(path.iterdir(), key=lambda p: (not p.is_dir(), p.name.lower())):
            if child.is_dir():
                item_type = ItemType.DIRECTORY
            else:
                item_type = classify_path(child)
            if item_type == ItemType.OTHER:
                continue
            stat = child.stat()
            items.append(
                VfsItem(
                    uri=file_uri(child),
                    display_name=child.name,
                    item_type=item_type,
                    size=stat.st_size,
                    compressed_size=stat.st_size,
                    modified_at=stat.st_mtime,
                )
            )
        if selected and all(item.uri != file_uri(selected) for item in items):
            stat = selected.stat()
            items.append(
                VfsItem(
                    uri=file_uri(selected),
                    display_name=selected.name,
                    item_type=ItemType.IMAGE,
                    size=stat.st_size,
                    compressed_size=stat.st_size,
                    modified_at=stat.st_mtime,
                )
            )
        return items

    def _list_archive_uri(self, uri: str) -> list[VfsItem]:
        parsed = parse_archive_uri(uri)
        archive_bytes = self.read_bytes(uri)
        entries = self.archive_reader.list(parsed.member_path, archive_bytes)
        return to_vfs_items(uri, entries)
