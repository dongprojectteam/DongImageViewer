#pragma once

#include "core/ImageLoader.h"
#include "core/Metadata.h"
#include "core/Vfs.h"
#include "core/VfsItem.h"

#include <QComboBox>
#include <QDockWidget>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QImage>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QPixmapCache>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTableWidget>
#include <QToolBar>
#include <QTimer>
#include <map>
#include <vector>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QString initialTarget = {}, QWidget* parent = nullptr);

private slots:
    void openDialog();
    void openFolderDialog();
    void openSelectedArchive();
    void selectIndex(int row);
    void nextImage();
    void previousImage();
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void actualSize();
    void rotateRight();
    void toggleFullscreen();
    void toggleSlideshow();
    void toggleBookmark();
    void applySort(int index);

private:
    Vfs vfs_;
    ImageLoader imageLoader_;
    std::vector<VfsItem> items_;
    int currentIndex_ = -1;
    QImage currentImage_;
    double zoom_ = 1.0;
    int rotation_ = 0;
    bool fit_ = true;
    QSet<QString> bookmarks_;

    QScrollArea* thumbnailScroll_ = nullptr;
    QWidget* thumbnailContainer_ = nullptr;
    QGridLayout* thumbnailLayout_ = nullptr;
    std::map<int, QPushButton*> thumbnailButtons_;
    QGraphicsView* imageView_ = nullptr;
    QGraphicsScene* imageScene_ = nullptr;
    QGraphicsPixmapItem* imageItem_ = nullptr;
    QTableWidget* metadataTable_ = nullptr;
    QDockWidget* metadataDock_ = nullptr;
    QComboBox* sortCombo_ = nullptr;
    QTimer* slideshowTimer_ = nullptr;
    QToolBar* commandBar_ = nullptr;

    void buildUi();
    void openTarget(const QString& target);
    void loadCurrentImage();
    void renderImage();
    void populateList();
    void updateThumbnailSelection(int row);
    void onThumbnailClicked(int index);
    void updateMetadata(const ImageMetadata& metadata);
    void updateItemDecoration(int row);
    void moveToImage(int direction);
    void prefetchAdjacentImages();
    void firstImage();
    void lastImage();
    void loadBookmarks();
    void saveBookmarks();
    QString itemPrefix(const VfsItem& item) const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};
