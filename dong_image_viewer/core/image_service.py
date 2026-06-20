from __future__ import annotations

import io

from PIL import Image, ImageOps

from .cache import LruMemoryCache
from .vfs import VirtualFileSystem


class ImageService:
    def __init__(self, vfs: VirtualFileSystem):
        self.vfs = vfs
        self.memory_cache = LruMemoryCache(max_items=32)

    def load_image(self, uri: str) -> Image.Image:
        cached = self.memory_cache.get(uri)
        if cached is not None:
            return cached.copy()
        data = self.vfs.read_bytes(uri)
        image = Image.open(io.BytesIO(data))
        image = ImageOps.exif_transpose(image).convert("RGBA")
        self.memory_cache.put(uri, image.copy())
        return image
