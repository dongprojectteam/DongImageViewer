# Dong Image Viewer

Dong Image Viewer는 이미지 파일과 압축 파일 내부 이미지를 하나의 탐색 경험으로 다루는 Windows 데스크톱 이미지 뷰어 MVP이다.

현재 구현은 요구사항/품질속성/설계 문서를 바탕으로 만든 Qt 6 + C++20 + CMake 데스크톱 앱이다. Python MVP는 초기 검증용 참고 구현과 회귀 테스트 자산으로 남겨두었다.

## 현재 지원 범위

- 로컬 폴더 이미지 탐색
- PNG, JPEG, WEBP, BMP, GIF 등 Pillow가 지원하는 이미지 디코딩
- ZIP, CBZ 내부 이미지 탐색
- 중첩 ZIP/CBZ 아카이브 기본 처리
- `file://`, `archive://` 기반 VFS
- 다음/이전 이미지 이동
- Fit-to-window, 1:1, 확대/축소, 90도 회전
- Zip bomb 의심 항목, 경로 탐색(`../`), 절대 경로 차단
- 중첩 아카이브 깊이 제한
- 메모리 LRU 이미지 캐시
- 썸네일 메모리 LRU + 디스크 영속 캐시
- 비동기 이미지/썸네일 로딩 및 다음·이전 프리패치
- PageUp/PageDown, Home/End 키보드 탐색
- 북마크 영속 저장 (설치형 AppData / 포터블 `data/`)
- `dongviewer://open?path=...` 프로토콜 핸들러
- `--rebuild-cache` 썸네일 캐시 재구성

## Qt/CMake 빌드

Qt가 기본 PATH에 없다면 아래처럼 Qt와 MinGW 경로를 먼저 추가한다.

```powershell
$env:Path='C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;' + $env:Path
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\mingw_64
cmake --build build --parallel
```

스모크 테스트:

```powershell
.\build\DongImageViewer.exe --smoke-test
```

앱 실행:

```powershell
.\build\DongImageViewer.exe
.\build\DongImageViewer.exe "D:\Comics\book.zip"
```

Qt 런타임 DLL까지 배포하려면 다음을 실행한다.

```powershell
windeployqt --release .\build\DongImageViewer.exe
```

배포 후에는 `build` 폴더의 `DongImageViewer.exe`를 PATH 설정 없이 실행할 수 있다.

## Qt 앱 구조

| 경로 | 역할 |
|---|---|
| `src/main.cpp` | 앱 진입점 |
| `src/MainWindow.*` | Qt Widgets 기반 이미지 뷰어 UI |
| `src/core/Vfs.*` | `file://`, `archive://` 통합 VFS |
| `src/core/ZipReader.*` | ZIP/CBZ 중앙 디렉터리 파싱 및 deflate/store 엔트리 읽기 |
| `src/core/ImageLoader.*` | VFS 바이트 스트림 디코딩, LRU 캐시, 프리패치 |
| `src/core/ThumbnailCache.*` | 썸네일 메모리 LRU + 디스크 캐시 |
| `src/core/AppPaths.*` | 설치형/포터블 데이터·캐시 경로 |
| `src/core/LruCache.h` | 범용 LRU 메모리 캐시 |
| `src/core/Uri.*` | URI 생성/파싱 |
| `src/core/ArchivePolicy.h` | 아카이브 보안/리소스 제한 정책 |

## Python 참고 MVP 실행 방법

```powershell
python -m pip install -r requirements.txt
python -m dong_image_viewer
```

특정 파일, 폴더, 아카이브를 바로 열 수 있다.

```powershell
python -m dong_image_viewer "D:\Images"
python -m dong_image_viewer "D:\Comics\book.zip"
```

## 테스트

```powershell
python -m unittest discover -s tests
```

## 문서

- 요구사항: `docs/requirements.md`
- 품질속성: `docs/quality-attributes.md`
- 설계 문서: `docs/architecture-design.md`

## 향후 확장 기준

현재 Qt 앱은 외부 의존성을 최소화하기 위해 ZIP/CBZ를 내장 리더로 처리한다. 최종 제품에서는 설계 문서에 따라 Archive Engine을 libarchive + 7-Zip SDK로 확장하고, Decode Pipeline은 libjpeg-turbo/libpng/libwebp/libavif 기반 전용 파이프라인으로 분리한다.
