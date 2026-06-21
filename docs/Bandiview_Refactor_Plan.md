
# Bandiview-Style Refactor Plan

## MainWindow.h 변경

```cpp
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QStackedWidget>

QGraphicsView* imageView_;
QGraphicsScene* imageScene_;
QGraphicsPixmapItem* imageItem_;

QDockWidget* thumbnailDock_;
QDockWidget* infoDock_;

QToolBar* commandBar_;
```

## UI 구조

```text
+-------------------------------------------------------+
| Open Prev Next Zoom Rotate Fullscreen                 |
+------------------+------------------------------------+
| Thumbnail Panel  |                                    |
|                  |      QGraphicsView Canvas          |
|                  |                                    |
+------------------+------------------------------------+
| Status Overlay                                     |
+-------------------------------------------------------+
```

## 핵심 변경

1. QLabel 제거
2. QScrollArea 제거
3. QGraphicsView 기반 렌더링
4. 썸네일 패널 고정
5. 다크 테마
6. SVG 아이콘
7. 전체화면 자동 숨김 UI
8. 애니메이션 줌

## 예시 렌더링

```cpp
imageScene_->clear();
auto pix = QPixmap::fromImage(currentImage_);
imageItem_ = imageScene_->addPixmap(pix);
imageView_->fitInView(imageItem_, Qt::KeepAspectRatio);
```
