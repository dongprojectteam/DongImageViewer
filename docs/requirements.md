아래는 지금까지 내용을 모두 반영해서 **“프레임워크까지 포함한 최종 요구사항”**으로 정리한 버전이다. (즉, 제품 요구사항 + 기술 스택 고정 포함)

---

# Windows Image Viewer + Archive Browser Requirements (Final + Framework Spec)

## 1. Overview

A high-performance Windows desktop application for viewing images and browsing images inside compressed archives without extraction. The system behaves like a hybrid between:

* Image viewer (fast navigation, GPU rendering)
* Archive utility (Bandizip-class browsing)

Optimized for large-scale image datasets such as comics, manga, and photo archives.

---

## 2. Goals

* Instant image rendering from filesystem and archives
* Seamless archive browsing without extraction
* Unified virtual file system abstraction
* High-performance navigation in large datasets
* Minimal latency (<100ms perception target)

---

## 3. Scope

### In Scope

* Local image viewing
* Archive browsing (ZIP, RAR, 7Z, TAR, GZ, ISO optional)
* Nested archives
* Thumbnail system with caching
* Basic image transforms (zoom, rotate, pan)

### Out of Scope

* Cloud sync
* Full image editing
* DAM workflows

---

## 4. Functional Requirements

## 4.1 Image Viewing

* Formats:

  * PNG, JPEG, WEBP, BMP, GIF (optional animation)
  * TIFF, AVIF (optional)
* GPU-accelerated rendering
* Progressive decoding for large images
* Smooth zoom/pan/rotate
* Fit-to-screen / 1:1 pixel view

---

## 4.2 Archive Support (Core)

* Open without extraction:

  * ZIP, RAR, 7Z, TAR, GZ, ISO (optional)
* Virtual folder representation
* Treat archive as navigable filesystem
* Nested archive support
* Sequential navigation across archive contents

---

## 4.3 Virtual File System (VFS)

* Unified abstraction:

  * Local filesystem
  * Archive contents
* URI scheme:

  * `file://`
  * `archive://`
* Transparent path resolution
* Metadata abstraction layer

---

## 4.4 Thumbnail System

* Async generation pipeline
* Persistent disk cache
* Memory LRU cache
* Prefetch next/previous images
* Adaptive resolution thumbnails

---

## 4.5 Navigation & UX

* Keyboard navigation (arrows, PageUp/Down)
* Mouse wheel zoom/navigation
* Slideshow mode
* Bookmark/favorites
* Index sidebar
* Multi-monitor support
* Minimal fullscreen overlay UI

---

## 4.6 Metadata Support

* EXIF parsing:

  * orientation
  * camera info
  * timestamp
* Sorting:

  * name, date, size, type
* Optional tagging/rating

---

## 5. Performance Requirements

* Archive open time: < 200ms (<500MB ZIP)
* Image decode: < 100ms (typical images)
* Thumbnail generation: async, non-blocking
* 60 FPS navigation minimum
* Strict memory limits (LRU eviction)
* Cold start < 1s (SSD target)

---

## 6. System Architecture

### 6.1 High-Level Modules

* UI Layer (Qt 6)
* Core Engine (C++)
* Archive Engine (libarchive + 7z SDK)
* Image Decode Engine (libjpeg-turbo, libpng, libwebp, libavif)
* Virtual File System (VFS)
* Thumbnail Cache Manager
* Indexing Service (background scanner)
* Plugin System
* Windows Shell Integration Layer

---

## 6.2 Data Flow

User Input
→ Windows Shell (optional entry point)
→ Qt UI Layer
→ VFS Layer
→ Archive Engine
→ Decode Pipeline
→ GPU Render Layer
→ UI

---

## 7. Security Considerations

* Zip bomb protection (compression ratio limits)
* Path traversal prevention (`../`)
* Resource caps:

  * max file count
  * max archive size
* sandboxed archive parsing
* graceful corruption handling

---

## 8. UX / UI Requirements

* Dual-pane mode (optional)
* Fullscreen immersive mode
* Minimal overlay UI
* Configurable shortcuts (vim-style optional)
* Drag & drop support
* Multi-monitor awareness

---

## 9. Extensibility

* Plugin architecture:

  * archive formats
  * image codecs
  * metadata providers
* Scriptable hooks (advanced)

---

## 10. Non-Functional Requirements

* Windows 10/11 support
* low idle CPU usage
* GPU acceleration preferred
* portable mode support
* fast cold start (<1s)

---

# 11. Windows Shell Integration

## 11.1 File Association

* Supported formats:

  * Images: PNG, JPG, JPEG, WEBP, BMP, GIF, TIFF, AVIF
  * Archives: ZIP, RAR, 7Z, TAR, GZ, ISO
* Default app registration
* Windows registry-based association
* restore previous associations on uninstall (optional)

---

## 11.2 Context Menu Integration

* Explorer right-click menu:

  * Open with Viewer
  * Browse Archive
  * View Next / Previous (folder context)
* Multi-selection batch open

---

## 11.3 Shell Extensions (Advanced)

* Thumbnail provider (Explorer integration)
* Preview handler (Explorer pane)
* COM-based context menu extension

---

## 11.4 Protocol Handler (Optional)

* `dongviewer://open?path=...`

---

# 12. FRAMEWORK & TECHNOLOGY STACK (NEW — FIXED)

## 12.1 Core Framework Choice (MANDATORY)

### ✔ Primary Application Framework

* **Qt 6.6+ (C++ 기반)**

#### UI Layer

* Qt Quick (QML) → preferred for performance + fluid UI
  or
* Qt Widgets → simpler but less modern

#### Rendering

* Qt SceneGraph
* OpenGL or Vulkan backend (platform dependent)

---

## 12.2 Core Engine (MANDATORY SEPARATION)

* Language: **C++20**
* Role:

  * VFS
  * Archive engine
  * Decode pipeline
  * thumbnail pipeline

---

## 12.3 Archive Engine

* libarchive (primary)
* 7-Zip SDK (for RAR/7Z edge cases)

---

## 12.4 Image Decode Stack

* libjpeg-turbo
* libpng
* libwebp
* libavif
* optional: libtiff

---

## 12.5 Concurrency / Performance

* Qt ThreadPool OR custom C++ thread pool
* lock-free queue for decode pipeline (preferred)
* async IO everywhere

---

## 12.6 Windows Integration Layer

* Win32 API (direct)
* COM (Shell extensions)
* registry manipulation layer

---

## 12.7 Build System

* CMake (mandatory)
* vcpkg or Conan for dependency management

---

## 12.8 Packaging / Installer Framework

### Primary Choice (RECOMMENDED)

* **Qt Installer Framework**

### Alternative

* Inno Setup (lightweight distribution)

### Installer Responsibilities

* file association
* registry setup
* shell context menu registration
* optional portable mode
* uninstall cleanup

---

## 13. Plugin System

* dynamic shared libraries (.dll)
* versioned plugin API
* sandbox optional (future)

---

## 14. Reliability Requirements

* corrupted archive handling
* partial file recovery
* cache rebuild mode

---

## 15. Deployment Modes

* Installer mode (Qt Installer Framework)
* Portable mode (no registry dependency)
* hybrid support

---

## 16. Future Extensions

* AI tagging
* OCR inside images
* cloud sync
* distributed cache system