#include "MainWindow.h"

#include "core/AppPaths.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTransform>
#include <QUrl>
#include <QWheelEvent>
#include <QtConcurrent>
#include <algorithm>

MainWindow::MainWindow(QString initialTarget, QWidget* parent)
    : QMainWindow(parent)
    , imageLoader_(vfs_)
{
    buildUi();
    loadBookmarks();
    setAcceptDrops(true);
    if (!initialTarget.isEmpty()) {
        openTarget(initialTarget);
    }
}

void MainWindow::buildUi()
{
    setWindowTitle("Dong Image Viewer");
    resize(1280, 820);

    auto* toolbar = addToolBar("Main");
    toolbar->addAction("Open", this, &MainWindow::openDialog)->setShortcut(QKeySequence::Open);
    toolbar->addAction("Folder", this, &MainWindow::openFolderDialog);
    toolbar->addAction("Prev", this, &MainWindow::previousImage)->setShortcut(Qt::Key_Left);
    toolbar->addAction("Next", this, &MainWindow::nextImage)->setShortcut(Qt::Key_Right);
    toolbar->addAction("Fit", this, &MainWindow::fitToWindow);
    toolbar->addAction("1:1", this, &MainWindow::actualSize);
    toolbar->addAction("Zoom +", this, &MainWindow::zoomIn)->setShortcut(QKeySequence::ZoomIn);
    toolbar->addAction("Zoom -", this, &MainWindow::zoomOut)->setShortcut(QKeySequence::ZoomOut);
    toolbar->addAction("Rotate", this, &MainWindow::rotateRight);
    toolbar->addAction("Fullscreen", this, &MainWindow::toggleFullscreen)->setShortcut(Qt::Key_F11);
    toolbar->addAction("Slideshow", this, &MainWindow::toggleSlideshow);
    toolbar->addAction("Bookmark", this, &MainWindow::toggleBookmark);

    sortCombo_ = new QComboBox(toolbar);
    sortCombo_->addItems({"Sort: Name", "Sort: Date", "Sort: Size", "Sort: Type"});
    connect(sortCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::applySort);
    toolbar->addWidget(sortCombo_);

    auto* splitter = new QSplitter(this);
    list_ = new QListWidget(splitter);
    list_->setMinimumWidth(260);
    list_->setMaximumWidth(420);
    list_->setIconSize(QSize(96, 96));
    connect(list_, &QListWidget::currentRowChanged, this, &MainWindow::selectIndex);
    connect(list_, &QListWidget::itemDoubleClicked, this, &MainWindow::openSelectedArchive);

    imageLabel_ = new QLabel;
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setBackgroundRole(QPalette::Dark);
    imageLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel_->setScaledContents(false);

    scrollArea_ = new QScrollArea(splitter);
    scrollArea_->setBackgroundRole(QPalette::Dark);
    scrollArea_->setWidget(imageLabel_);
    scrollArea_->setWidgetResizable(false);
    scrollArea_->setAlignment(Qt::AlignCenter);

    splitter->addWidget(list_);
    splitter->addWidget(scrollArea_);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    metadataTable_ = new QTableWidget(0, 2, this);
    metadataTable_->setHorizontalHeaderLabels({"Key", "Value"});
    metadataTable_->horizontalHeader()->setStretchLastSection(true);
    metadataTable_->verticalHeader()->hide();
    metadataTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    metadataTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    auto* metadataDock = new QDockWidget("Metadata", this);
    metadataDock->setWidget(metadataTable_);
    addDockWidget(Qt::RightDockWidgetArea, metadataDock);

    slideshowTimer_ = new QTimer(this);
    slideshowTimer_->setInterval(3000);
    connect(slideshowTimer_, &QTimer::timeout, this, &MainWindow::nextImage);

    statusBar()->showMessage("Open an image, folder, ZIP, or CBZ archive.");
}

void MainWindow::openDialog()
{
    const QString target = QFileDialog::getOpenFileName(
        this,
        "Open image or archive",
        {},
        "Images and archives (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.tif *.tiff *.avif *.zip *.cbz);;All files (*.*)");
    if (!target.isEmpty()) {
        openTarget(target);
    }
}

void MainWindow::openFolderDialog()
{
    const QString target = QFileDialog::getExistingDirectory(this, "Open folder");
    if (!target.isEmpty()) {
        openTarget(target);
    }
}

void MainWindow::openTarget(const QString& target)
{
    try {
        items_ = vfs_.openCollection(target);
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Open failed", ex.what());
        return;
    }

    applySort(sortCombo_ ? sortCombo_->currentIndex() : 0);

    currentIndex_ = -1;
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (items_[i].isImage()) {
            list_->setCurrentRow(i);
            return;
        }
    }
    currentImage_ = {};
    imageLabel_->clear();
    statusBar()->showMessage(QString("%1 browsable items loaded.").arg(items_.size()));
}

void MainWindow::openSelectedArchive()
{
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(items_.size())) {
        return;
    }
    if (items_[currentIndex_].isArchive()) {
        openTarget(items_[currentIndex_].uri);
    }
}

void MainWindow::selectIndex(int row)
{
    if (row < 0 || row >= static_cast<int>(items_.size())) {
        return;
    }
    currentIndex_ = row;
    if (items_[row].isImage()) {
        loadCurrentImage();
    }
}

void MainWindow::loadCurrentImage()
{
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(items_.size())) {
        return;
    }

    const int requestIndex = currentIndex_;
    const VfsItem item = items_[requestIndex];
    statusBar()->showMessage(QString("Loading %1...").arg(item.displayName));

    (void)QtConcurrent::run([this, requestIndex, item]() {
        ImageData data;
        try {
            data = imageLoader_.loadWithMetadata(item.uri);
        } catch (const std::exception& ex) {
            const QString message = QString::fromStdString(ex.what());
            QMetaObject::invokeMethod(this, [this, requestIndex, message]() {
                if (requestIndex != currentIndex_) {
                    return;
                }
                QMessageBox::warning(this, "Decode failed", message);
            }, Qt::QueuedConnection);
            return;
        }

        QMetaObject::invokeMethod(this, [this, requestIndex, data]() {
            if (requestIndex != currentIndex_) {
                return;
            }
            currentImage_ = data.image;
            renderImage();
            updateMetadata(data.metadata);
            updateItemDecoration(requestIndex);
            statusBar()->showMessage(QString("%1/%2  %3  %4x%5")
                .arg(currentIndex_ + 1)
                .arg(items_.size())
                .arg(items_[currentIndex_].displayName)
                .arg(currentImage_.width())
                .arg(currentImage_.height()));
            prefetchAdjacentImages();
        }, Qt::QueuedConnection);
    });
}

void MainWindow::renderImage()
{
    if (currentImage_.isNull()) {
        return;
    }
    QImage image = currentImage_;
    if (rotation_ != 0) {
        QTransform transform;
        transform.rotate(rotation_);
        image = image.transformed(transform, Qt::SmoothTransformation);
    }

    double scale = zoom_;
    if (fit_) {
        const QSize viewport = scrollArea_->viewport()->size();
        scale = std::min(
            viewport.width() / static_cast<double>(image.width()),
            viewport.height() / static_cast<double>(image.height()));
        scale = std::min(scale, 1.0);
    }
    const QSize targetSize(
        std::max(1, static_cast<int>(image.width() * scale)),
        std::max(1, static_cast<int>(image.height() * scale)));
    const QPixmap pixmap = QPixmap::fromImage(image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    imageLabel_->setPixmap(pixmap);
    imageLabel_->resize(targetSize);
}

void MainWindow::populateList()
{
    list_->clear();
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const VfsItem& item = items_[i];
        auto* listItem = new QListWidgetItem(itemPrefix(item) + " " + item.displayName);
        listItem->setToolTip(item.uri);
        list_->addItem(listItem);
        updateItemDecoration(i);
    }
}

void MainWindow::updateMetadata(const ImageMetadata& metadata)
{
    metadataTable_->setRowCount(0);
    auto addRow = [this](const QString& key, const QString& value) {
        const int row = metadataTable_->rowCount();
        metadataTable_->insertRow(row);
        metadataTable_->setItem(row, 0, new QTableWidgetItem(key));
        metadataTable_->setItem(row, 1, new QTableWidgetItem(value));
    };

    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(items_.size())) {
        const VfsItem& item = items_[currentIndex_];
        addRow("Name", item.displayName);
        addRow("URI", item.uri);
        addRow("Size", QString::number(item.size));
        addRow("Compressed Size", QString::number(item.compressedSize));
        if (item.modifiedAt > 0) {
            addRow("Modified", QDateTime::fromMSecsSinceEpoch(item.modifiedAt).toString(Qt::ISODate));
        }
        addRow("Bookmarked", bookmarks_.contains(item.uri) ? "Yes" : "No");
    }
    for (const MetadataEntry& entry : metadata.entries) {
        addRow(entry.key, entry.value);
    }
    metadataTable_->resizeColumnsToContents();
}

void MainWindow::updateItemDecoration(int row)
{
    if (row < 0 || row >= list_->count() || row >= static_cast<int>(items_.size())) {
        return;
    }
    QListWidgetItem* listItem = list_->item(row);
    const VfsItem& item = items_[row];
    QString label = itemPrefix(item) + " " + item.displayName;
    if (bookmarks_.contains(item.uri)) {
        label = "* " + label;
    }
    listItem->setText(label);
    if (item.isImage()) {
        (void)QtConcurrent::run([this, row, item]() {
            try {
                const QImage thumb = imageLoader_.loadThumbnail(item);
                QMetaObject::invokeMethod(this, [this, row, thumb]() {
                    if (row < 0 || row >= list_->count()) {
                        return;
                    }
                    list_->item(row)->setIcon(QIcon(QPixmap::fromImage(
                        thumb.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
                }, Qt::QueuedConnection);
            } catch (...) {
            }
        });
    }
}

void MainWindow::moveToImage(int direction)
{
    if (items_.empty()) {
        return;
    }
    int index = currentIndex_;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        index = (index + direction + static_cast<int>(items_.size())) % static_cast<int>(items_.size());
        if (items_[index].isImage()) {
            list_->setCurrentRow(index);
            return;
        }
    }
}

void MainWindow::nextImage()
{
    moveToImage(1);
}

void MainWindow::previousImage()
{
    moveToImage(-1);
}

void MainWindow::zoomIn()
{
    fit_ = false;
    zoom_ = std::min(zoom_ * 1.2, 8.0);
    renderImage();
}

void MainWindow::zoomOut()
{
    fit_ = false;
    zoom_ = std::max(zoom_ / 1.2, 0.05);
    renderImage();
}

void MainWindow::fitToWindow()
{
    fit_ = true;
    zoom_ = 1.0;
    renderImage();
}

void MainWindow::actualSize()
{
    fit_ = false;
    zoom_ = 1.0;
    renderImage();
}

void MainWindow::rotateRight()
{
    rotation_ = (rotation_ + 90) % 360;
    renderImage();
}

void MainWindow::toggleFullscreen()
{
    isFullScreen() ? showNormal() : showFullScreen();
}

void MainWindow::toggleSlideshow()
{
    if (slideshowTimer_->isActive()) {
        slideshowTimer_->stop();
        statusBar()->showMessage("Slideshow stopped.");
    } else {
        slideshowTimer_->start();
        statusBar()->showMessage("Slideshow started.");
    }
}

void MainWindow::toggleBookmark()
{
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(items_.size())) {
        return;
    }
    const QString uri = items_[currentIndex_].uri;
    if (bookmarks_.contains(uri)) {
        bookmarks_.remove(uri);
    } else {
        bookmarks_.insert(uri);
    }
    updateItemDecoration(currentIndex_);
    ImageMetadata empty;
    updateMetadata(empty);
    saveBookmarks();
}

void MainWindow::applySort(int index)
{
    if (index == 1) {
        std::stable_sort(items_.begin(), items_.end(), [](const VfsItem& a, const VfsItem& b) {
            return a.modifiedAt > b.modifiedAt;
        });
    } else if (index == 2) {
        std::stable_sort(items_.begin(), items_.end(), [](const VfsItem& a, const VfsItem& b) {
            return a.size < b.size;
        });
    } else if (index == 3) {
        std::stable_sort(items_.begin(), items_.end(), [](const VfsItem& a, const VfsItem& b) {
            if (a.type == b.type) {
                return a.displayName.toLower() < b.displayName.toLower();
            }
            return static_cast<int>(a.type) < static_cast<int>(b.type);
        });
    } else {
        std::stable_sort(items_.begin(), items_.end(), [](const VfsItem& a, const VfsItem& b) {
            return a.displayName.toLower() < b.displayName.toLower();
        });
    }
    populateList();
}

QString MainWindow::itemPrefix(const VfsItem& item) const
{
    if (item.isArchive()) {
        return "[A]";
    }
    if (item.isImage()) {
        return "[I]";
    }
    return "[D]";
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (fit_) {
        renderImage();
    }
}

void MainWindow::wheelEvent(QWheelEvent* event)
{
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        event->angleDelta().y() > 0 ? zoomIn() : zoomOut();
    } else {
        event->angleDelta().y() > 0 ? previousImage() : nextImage();
    }
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) {
        openTarget(urls.first().toLocalFile());
    }
}

void MainWindow::loadBookmarks()
{
    QFile file(AppPaths::bookmarksFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.isEmpty()) {
            bookmarks_.insert(line);
        }
    }
}

void MainWindow::saveBookmarks()
{
    QFile file(AppPaths::bookmarksFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    for (const QString& uri : bookmarks_) {
        file.write(uri.toUtf8());
        file.write("\n");
    }
}

void MainWindow::prefetchAdjacentImages()
{
    if (items_.empty() || currentIndex_ < 0) {
        return;
    }
    std::vector<QString> uris;
    int index = currentIndex_;
    for (int step = -1; step <= 1; step += 2) {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            index = (index + step + static_cast<int>(items_.size())) % static_cast<int>(items_.size());
            if (items_[index].isImage()) {
                uris.push_back(items_[index].uri);
                break;
            }
        }
    }
    imageLoader_.prefetch(uris);
}

void MainWindow::firstImage()
{
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (items_[i].isImage()) {
            list_->setCurrentRow(i);
            return;
        }
    }
}

void MainWindow::lastImage()
{
    for (int i = static_cast<int>(items_.size()) - 1; i >= 0; --i) {
        if (items_[i].isImage()) {
            list_->setCurrentRow(i);
            return;
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_PageUp:
        previousImage();
        event->accept();
        return;
    case Qt::Key_PageDown:
        nextImage();
        event->accept();
        return;
    case Qt::Key_Home:
        firstImage();
        event->accept();
        return;
    case Qt::Key_End:
        lastImage();
        event->accept();
        return;
    case Qt::Key_Escape:
        if (isFullScreen()) {
            showNormal();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }
    QMainWindow::keyPressEvent(event);
}
