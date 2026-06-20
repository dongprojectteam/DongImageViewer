from __future__ import annotations

import io
import tarfile
import zipfile
from dataclasses import dataclass
from pathlib import Path

from .models import ItemType, VfsItem, classify_path
from .security import ArchivePolicy, SecurityError, validate_archive_entry
from .uri import archive_uri


@dataclass(frozen=True)
class ArchiveEntry:
    path: str
    item_type: ItemType
    size: int
    compressed_size: int
    modified_at: float | None = None


class ArchiveReader:
    def __init__(self, policy: ArchivePolicy | None = None):
        self.policy = policy or ArchivePolicy()

    def list(self, archive_ref: str, data: bytes | None = None) -> list[ArchiveEntry]:
        if data is None:
            path = Path(archive_ref)
            suffix = path.name.lower()
            if suffix.endswith(".zip") or suffix.endswith(".cbz"):
                with zipfile.ZipFile(path) as zf:
                    return self._list_zip(zf)
            if suffix.endswith(".tar") or suffix.endswith(".tgz") or suffix.endswith(".tar.gz"):
                with tarfile.open(path) as tf:
                    return self._list_tar(tf)
        else:
            lower = archive_ref.lower()
            stream = io.BytesIO(data)
            if lower.endswith(".zip") or lower.endswith(".cbz"):
                with zipfile.ZipFile(stream) as zf:
                    return self._list_zip(zf)
            if lower.endswith(".tar") or lower.endswith(".tgz") or lower.endswith(".tar.gz"):
                with tarfile.open(fileobj=stream) as tf:
                    return self._list_tar(tf)
        raise ValueError(f"Unsupported archive format: {archive_ref}")

    def read_member(self, archive_ref: str, member_path: str, data: bytes | None = None) -> bytes:
        if data is None:
            lower = Path(archive_ref).name.lower()
            if lower.endswith(".zip") or lower.endswith(".cbz"):
                with zipfile.ZipFile(archive_ref) as zf:
                    info = zf.getinfo(member_path)
                    validate_archive_entry(
                        member_name=info.filename,
                        uncompressed_size=info.file_size,
                        compressed_size=info.compress_size,
                        policy=self.policy,
                    )
                    return zf.read(info)
            if lower.endswith(".tar") or lower.endswith(".tgz") or lower.endswith(".tar.gz"):
                with tarfile.open(archive_ref) as tf:
                    return self._read_tar_member(tf, member_path)
        else:
            lower = archive_ref.lower()
            stream = io.BytesIO(data)
            if lower.endswith(".zip") or lower.endswith(".cbz"):
                with zipfile.ZipFile(stream) as zf:
                    info = zf.getinfo(member_path)
                    validate_archive_entry(
                        member_name=info.filename,
                        uncompressed_size=info.file_size,
                        compressed_size=info.compress_size,
                        policy=self.policy,
                    )
                    return zf.read(info)
            if lower.endswith(".tar") or lower.endswith(".tgz") or lower.endswith(".tar.gz"):
                with tarfile.open(fileobj=stream) as tf:
                    return self._read_tar_member(tf, member_path)
        raise ValueError(f"Unsupported archive format: {archive_ref}")

    def _list_zip(self, zf: zipfile.ZipFile) -> list[ArchiveEntry]:
        infos = zf.infolist()
        if len(infos) > self.policy.max_entries:
            raise SecurityError("Archive has too many entries")

        total_size = 0
        entries: list[ArchiveEntry] = []
        for info in infos:
            if info.is_dir():
                continue
            safe_name = validate_archive_entry(
                member_name=info.filename,
                uncompressed_size=info.file_size,
                compressed_size=info.compress_size,
                policy=self.policy,
            )
            total_size += info.file_size
            if total_size > self.policy.max_uncompressed_size:
                raise SecurityError("Archive uncompressed size limit exceeded")
            entries.append(
                ArchiveEntry(
                    path=safe_name,
                    item_type=classify_path(safe_name),
                    size=info.file_size,
                    compressed_size=info.compress_size,
                )
            )
        return entries

    def _list_tar(self, tf: tarfile.TarFile) -> list[ArchiveEntry]:
        members = tf.getmembers()
        if len(members) > self.policy.max_entries:
            raise SecurityError("Archive has too many entries")

        total_size = 0
        entries: list[ArchiveEntry] = []
        for member in members:
            if not member.isfile():
                continue
            safe_name = validate_archive_entry(
                member_name=member.name,
                uncompressed_size=member.size,
                compressed_size=max(member.size, 1),
                policy=self.policy,
            )
            total_size += member.size
            if total_size > self.policy.max_uncompressed_size:
                raise SecurityError("Archive uncompressed size limit exceeded")
            entries.append(
                ArchiveEntry(
                    path=safe_name,
                    item_type=classify_path(safe_name),
                    size=member.size,
                    compressed_size=member.size,
                    modified_at=member.mtime,
                )
            )
        return entries

    def _read_tar_member(self, tf: tarfile.TarFile, member_path: str) -> bytes:
        member = tf.getmember(member_path)
        validate_archive_entry(
            member_name=member.name,
            uncompressed_size=member.size,
            compressed_size=max(member.size, 1),
            policy=self.policy,
        )
        extracted = tf.extractfile(member)
        if extracted is None:
            raise FileNotFoundError(member_path)
        return extracted.read()


def to_vfs_items(archive_ref_uri: str, entries: list[ArchiveEntry]) -> list[VfsItem]:
    return [
        VfsItem(
            uri=archive_uri(archive_ref_uri, entry.path),
            display_name=entry.path,
            item_type=entry.item_type,
            size=entry.size,
            compressed_size=entry.compressed_size,
            modified_at=entry.modified_at,
        )
        for entry in entries
        if entry.item_type in {ItemType.IMAGE, ItemType.ARCHIVE}
    ]
