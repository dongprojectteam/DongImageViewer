#pragma once

#include "core/ImageLoader.h"
#include "core/Metadata.h"
#include "core/Vfs.h"
#include "core/VfsItem.h"

#include <QComboBox>
#include <QDockWidget>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QScrollArea>
#include <QSet>
#include <QTableWidget>
#include <QToolBar>
#include <QTimer>
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

    QListWidget* list_ = nullptr;
    QLabel* imageLabel_ = nullptr;
    QScrollArea* scrollArea_ = nullptr;
    QTableWidget* metadataTable_ = nullptr;
    QComboBox* sortCombo_ = nullptr;
    QTimer* slideshowTimer_ = nullptr;

    void buildUi();
    void openTarget(const QString& target);
    void loadCurrentImage();
    void renderImage();
    void populateList();
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
