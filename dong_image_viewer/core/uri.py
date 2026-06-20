from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from urllib.parse import quote, unquote, urlparse


@dataclass(frozen=True)
class ArchiveUri:
    archive_uri: str
    member_path: str


def file_uri(path: str | Path) -> str:
    return Path(path).resolve().as_uri()


def archive_uri(archive: str, member_path: str) -> str:
    return f"archive://{quote(archive, safe=':/%!')}!/{quote(member_path, safe='/')}"


def is_archive_uri(uri: str) -> bool:
    return uri.startswith("archive://")


def parse_file_uri(uri: str) -> Path:
    parsed = urlparse(uri)
    if parsed.scheme != "file":
        return Path(uri).resolve()
    return Path(unquote(parsed.path.lstrip("/")) if parsed.netloc == "" else f"//{parsed.netloc}{unquote(parsed.path)}")


def parse_archive_uri(uri: str) -> ArchiveUri:
    if not is_archive_uri(uri):
        raise ValueError(f"Not an archive URI: {uri}")
    payload = uri[len("archive://") :]
    archive_part, separator, member_part = payload.rpartition("!/")
    if not separator:
        raise ValueError(f"Invalid archive URI: {uri}")
    return ArchiveUri(unquote(archive_part), unquote(member_part))
