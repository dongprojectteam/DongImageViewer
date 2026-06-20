cmake_minimum_required(VERSION 3.21)

project(DongImageViewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

qt_standard_project_setup()

qt_add_executable(DongImageViewer
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/core/AppPaths.cpp
    src/core/AppPaths.h
    src/core/ArchivePolicy.h
    src/core/ImageLoader.cpp
    src/core/ImageData.h
    src/core/ImageLoader.h
    src/core/LruCache.h
    src/core/Metadata.h
    src/core/ThumbnailCache.cpp
    src/core/ThumbnailCache.h
    src/core/Uri.cpp
    src/core/Uri.h
    src/core/Vfs.cpp
    src/core/Vfs.h
    src/core/VfsItem.h
    src/core/ZipReader.cpp
    src/core/ZipReader.h
)

target_link_libraries(DongImageViewer
    PRIVATE
        Qt6::Widgets
        Qt6::Concurrent
        z
)

if(WIN32)
    set_target_properties(DongImageViewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
