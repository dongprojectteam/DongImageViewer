from __future__ import annotations

import argparse
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from PIL import Image, ImageTk

from .core.image_service import ImageService
from .core.models import ItemType, VfsItem
from .core.vfs import VirtualFileSystem


class ViewerApp(tk.Tk):
    def __init__(self, initial_path: str | None = None):
        super().__init__()
        self.title("Dong Image Viewer")
        self.geometry("1200x800")
        self.minsize(820, 520)

        self.vfs = VirtualFileSystem()
        self.image_service = ImageService(self.vfs)

        self.items: list[VfsItem] = []
        self.current_index = -1
        self.current_image: Image.Image | None = None
        self.rendered_image: ImageTk.PhotoImage | None = None
        self.zoom = 1.0
        self.rotation = 0
        self.fit_to_window = True

        self._build_ui()
        self._bind_keys()

        if initial_path:
            self.open_target(initial_path)

    def _build_ui(self) -> None:
        self.columnconfigure(1, weight=1)
        self.rowconfigure(1, weight=1)

        toolbar = ttk.Frame(self, padding=(6, 4))
        toolbar.grid(row=0, column=0, columnspan=2, sticky="ew")

        ttk.Button(toolbar, text="Open", command=self.open_dialog).pack(side="left")
        ttk.Button(toolbar, text="Prev", command=self.prev_image).pack(side="left", padx=(6, 0))
        ttk.Button(toolbar, text="Next", command=self.next_image).pack(side="left")
        ttk.Button(toolbar, text="Fit", command=self.toggle_fit).pack(side="left", padx=(6, 0))
        ttk.Button(toolbar, text="1:1", command=self.actual_size).pack(side="left")
        ttk.Button(toolbar, text="Rotate", command=self.rotate_right).pack(side="left")

        self.status_var = tk.StringVar(value="Open an image, folder, ZIP, CBZ, TAR, or TGZ file.")
        ttk.Label(toolbar, textvariable=self.status_var, anchor="w").pack(side="left", padx=(12, 0), fill="x", expand=True)

        self.listbox = tk.Listbox(self, width=34, activestyle="dotbox")
        self.listbox.grid(row=1, column=0, sticky="nsew")
        self.listbox.bind("<<ListboxSelect>>", self._on_listbox_select)
        self.listbox.bind("<Double-Button-1>", self._on_listbox_open)

        self.canvas = tk.Canvas(self, bg="#111111", highlightthickness=0)
        self.canvas.grid(row=1, column=1, sticky="nsew")
        self.canvas.bind("<Configure>", lambda _event: self.render_current())
        self.canvas.bind("<MouseWheel>", self._on_mouse_wheel)

    def _bind_keys(self) -> None:
        self.bind("<Left>", lambda _event: self.prev_image())
        self.bind("<Right>", lambda _event: self.next_image())
        self.bind("<Prior>", lambda _event: self.prev_image())
        self.bind("<Next>", lambda _event: self.next_image())
        self.bind("<Control-o>", lambda _event: self.open_dialog())
        self.bind("<plus>", lambda _event: self.zoom_in())
        self.bind("<minus>", lambda _event: self.zoom_out())
        self.bind("<Escape>", lambda _event: self.attributes("-fullscreen", False))
        self.bind("<F11>", lambda _event: self.attributes("-fullscreen", not self.attributes("-fullscreen")))

    def open_dialog(self) -> None:
        target = filedialog.askopenfilename(
            title="Open image or archive",
            filetypes=[
                ("Images and archives", "*.png *.jpg *.jpeg *.webp *.bmp *.gif *.tif *.tiff *.avif *.zip *.cbz *.tar *.tgz *.tar.gz"),
                ("All files", "*.*"),
            ],
        )
        if target:
            self.open_target(target)

    def open_target(self, target: str) -> None:
        try:
            self.items = self.vfs.open_collection(target)
        except Exception as exc:
            messagebox.showerror("Open failed", str(exc))
            return

        self.listbox.delete(0, tk.END)
        for item in self.items:
            prefix = "[A]" if item.item_type == ItemType.ARCHIVE else "[I]" if item.item_type == ItemType.IMAGE else "[D]"
            self.listbox.insert(tk.END, f"{prefix} {item.display_name}")

        first_image = next((i for i, item in enumerate(self.items) if item.is_image), -1)
        self.current_index = first_image
        self.zoom = 1.0
        self.rotation = 0
        self.fit_to_window = True
        if first_image >= 0:
            self.listbox.selection_clear(0, tk.END)
            self.listbox.selection_set(first_image)
            self.listbox.see(first_image)
            self.load_current_async()
        else:
            self.current_image = None
            self.canvas.delete("all")
            self.status_var.set(f"{len(self.items)} browsable items loaded.")

    def _on_listbox_select(self, _event: tk.Event) -> None:
        selection = self.listbox.curselection()
        if not selection:
            return
        index = int(selection[0])
        self.current_index = index
        item = self.items[index]
        if item.is_image:
            self.load_current_async()

    def _on_listbox_open(self, _event: tk.Event) -> None:
        if not (0 <= self.current_index < len(self.items)):
            return
        item = self.items[self.current_index]
        if item.is_archive:
            self.open_target(item.uri)

    def load_current_async(self) -> None:
        if not (0 <= self.current_index < len(self.items)):
            return
        item = self.items[self.current_index]
        self.status_var.set(f"Loading {item.display_name}...")

        def worker() -> None:
            try:
                image = self.image_service.load_image(item.uri)
            except Exception as exc:
                self.after(0, lambda: messagebox.showerror("Decode failed", str(exc)))
                return
            self.after(0, lambda: self._set_current_image(image))

        threading.Thread(target=worker, daemon=True).start()

    def _set_current_image(self, image: Image.Image) -> None:
        self.current_image = image
        self.render_current()
        item = self.items[self.current_index]
        self.status_var.set(f"{self.current_index + 1}/{len(self.items)}  {item.display_name}  {image.width}x{image.height}")

    def render_current(self) -> None:
        self.canvas.delete("all")
        if self.current_image is None:
            return

        image = self.current_image
        if self.rotation:
            image = image.rotate(-self.rotation, expand=True)

        canvas_width = max(self.canvas.winfo_width(), 1)
        canvas_height = max(self.canvas.winfo_height(), 1)
        scale = self.zoom
        if self.fit_to_window:
            scale = min(canvas_width / image.width, canvas_height / image.height, 1.0)

        width = max(int(image.width * scale), 1)
        height = max(int(image.height * scale), 1)
        resized = image.resize((width, height), Image.Resampling.LANCZOS)
        self.rendered_image = ImageTk.PhotoImage(resized)
        self.canvas.create_image(canvas_width // 2, canvas_height // 2, image=self.rendered_image, anchor="center")

    def _move_to_image(self, direction: int) -> None:
        if not self.items:
            return
        index = self.current_index
        for _ in range(len(self.items)):
            index = (index + direction) % len(self.items)
            if self.items[index].is_image:
                self.current_index = index
                self.listbox.selection_clear(0, tk.END)
                self.listbox.selection_set(index)
                self.listbox.see(index)
                self.load_current_async()
                return

    def prev_image(self) -> None:
        self._move_to_image(-1)

    def next_image(self) -> None:
        self._move_to_image(1)

    def toggle_fit(self) -> None:
        self.fit_to_window = True
        self.zoom = 1.0
        self.render_current()

    def actual_size(self) -> None:
        self.fit_to_window = False
        self.zoom = 1.0
        self.render_current()

    def zoom_in(self) -> None:
        self.fit_to_window = False
        self.zoom = min(self.zoom * 1.2, 8.0)
        self.render_current()

    def zoom_out(self) -> None:
        self.fit_to_window = False
        self.zoom = max(self.zoom / 1.2, 0.05)
        self.render_current()

    def rotate_right(self) -> None:
        self.rotation = (self.rotation + 90) % 360
        self.render_current()

    def _on_mouse_wheel(self, event: tk.Event) -> None:
        if event.state & 0x0004:
            if event.delta > 0:
                self.zoom_in()
            else:
                self.zoom_out()
        elif event.delta > 0:
            self.prev_image()
        else:
            self.next_image()


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Dong Image Viewer MVP")
    parser.add_argument("path", nargs="?", help="Image, folder, or archive to open")
    args = parser.parse_args(argv)

    initial_path = str(Path(args.path).resolve()) if args.path else None
    app = ViewerApp(initial_path=initial_path)
    app.mainloop()
