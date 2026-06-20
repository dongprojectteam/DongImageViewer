from __future__ import annotations

from dataclasses import dataclass
from pathlib import PurePosixPath


class SecurityError(RuntimeError):
    """Raised when an input violates archive or path safety policy."""


@dataclass(frozen=True)
class ArchivePolicy:
    max_entries: int = 20_000
    max_uncompressed_size: int = 2 * 1024 * 1024 * 1024
    max_single_file_size: int = 512 * 1024 * 1024
    max_compression_ratio: int = 200
    max_nested_depth: int = 4


def normalize_archive_member(name: str) -> str:
    normalized = name.replace("\\", "/").strip()
    if not normalized:
        raise SecurityError("Empty archive member path")

    path = PurePosixPath(normalized)
    if path.is_absolute():
        raise SecurityError(f"Absolute archive path is not allowed: {name}")
    if any(part in {"..", ""} for part in path.parts):
        raise SecurityError(f"Unsafe archive path is not allowed: {name}")
    if len(path.parts) > 256:
        raise SecurityError(f"Archive path is too deep: {name}")
    return path.as_posix()


def validate_archive_entry(
    *,
    member_name: str,
    uncompressed_size: int,
    compressed_size: int,
    policy: ArchivePolicy,
) -> str:
    safe_name = normalize_archive_member(member_name)
    if uncompressed_size > policy.max_single_file_size:
        raise SecurityError(f"Archive member is too large: {safe_name}")
    if compressed_size > 0 and uncompressed_size / compressed_size > policy.max_compression_ratio:
        raise SecurityError(f"Suspicious compression ratio: {safe_name}")
    return safe_name
