from __future__ import annotations

import hashlib
from collections import OrderedDict
from pathlib import Path
from tempfile import gettempdir


class LruMemoryCache:
    def __init__(self, max_items: int = 128):
        self.max_items = max_items
        self._items: OrderedDict[str, object] = OrderedDict()

    def get(self, key: str) -> object | None:
        if key not in self._items:
            return None
        value = self._items.pop(key)
        self._items[key] = value
        return value

    def put(self, key: str, value: object) -> None:
        if key in self._items:
            self._items.pop(key)
        self._items[key] = value
        while len(self._items) > self.max_items:
            self._items.popitem(last=False)


class DiskCache:
    def __init__(self, root: Path | None = None):
        self.root = root or Path(gettempdir()) / "dong_image_viewer_cache"
        self.root.mkdir(parents=True, exist_ok=True)

    def path_for(self, key: str, suffix: str = ".png") -> Path:
        digest = hashlib.sha256(key.encode("utf-8")).hexdigest()
        return self.root / f"{digest}{suffix}"
