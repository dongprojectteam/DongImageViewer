from __future__ import annotations

import tempfile
import unittest
import zipfile
from pathlib import Path

from PIL import Image

from dong_image_viewer.core.image_service import ImageService
from dong_image_viewer.core.models import ItemType
from dong_image_viewer.core.security import SecurityError, normalize_archive_member
from dong_image_viewer.core.vfs import VirtualFileSystem


class CoreTests(unittest.TestCase):
    def test_local_directory_lists_images_and_archives(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            Image.new("RGB", (8, 8), "red").save(root / "a.png")
            (root / "note.txt").write_text("ignored", encoding="utf-8")
            with zipfile.ZipFile(root / "book.zip", "w") as zf:
                zf.writestr("001.png", b"not decoded here")

            items = VirtualFileSystem().open_collection(root)
            names = {item.display_name: item.item_type for item in items}

            self.assertEqual(names["a.png"], ItemType.IMAGE)
            self.assertEqual(names["book.zip"], ItemType.ARCHIVE)
            self.assertNotIn("note.txt", names)

    def test_zip_archive_lists_image_entries(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "book.zip"
            with zipfile.ZipFile(archive, "w") as zf:
                zf.writestr("chapter/001.jpg", b"fake")
                zf.writestr("chapter/readme.txt", b"ignored")

            items = VirtualFileSystem().open_collection(archive)

            self.assertEqual(len(items), 1)
            self.assertEqual(items[0].display_name, "chapter/001.jpg")
            self.assertEqual(items[0].item_type, ItemType.IMAGE)

    def test_unsafe_archive_paths_are_rejected(self) -> None:
        with self.assertRaises(SecurityError):
            normalize_archive_member("../escape.png")
        with self.assertRaises(SecurityError):
            normalize_archive_member("/absolute/escape.png")

    def test_image_service_decodes_zip_member(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.png"
            Image.new("RGB", (12, 10), "blue").save(source)

            archive = root / "book.zip"
            with zipfile.ZipFile(archive, "w") as zf:
                zf.write(source, "001.png")

            vfs = VirtualFileSystem()
            items = vfs.open_collection(archive)
            image = ImageService(vfs).load_image(items[0].uri)

            self.assertEqual(image.size, (12, 10))

    def test_nested_zip_archive_can_be_opened(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.png"
            inner = root / "inner.zip"
            outer = root / "outer.zip"
            Image.new("RGB", (6, 5), "green").save(source)

            with zipfile.ZipFile(inner, "w") as zf:
                zf.write(source, "nested/001.png")
            with zipfile.ZipFile(outer, "w") as zf:
                zf.write(inner, "inner.zip")

            vfs = VirtualFileSystem()
            outer_items = vfs.open_collection(outer)
            self.assertEqual(outer_items[0].item_type, ItemType.ARCHIVE)

            inner_items = vfs.open_collection(outer_items[0].uri)
            self.assertEqual(inner_items[0].display_name, "nested/001.png")
            image = ImageService(vfs).load_image(inner_items[0].uri)
            self.assertEqual(image.size, (6, 5))


if __name__ == "__main__":
    unittest.main()
