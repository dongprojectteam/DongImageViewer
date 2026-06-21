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
    resize(1440, 900);

    qApp->setStyleSheet(R"(
        QMainWindow { background:#1e1f22; }
        QToolBar { background:#2b2d31; border:none; spacing:6px; padding:8px; }
        QToolButton { color:white; padding:6px 10px; border-radius:6px; }
        QToolButton:hover { background:#3b3f45; }
        QListWidget { background:#25262b; color:white; border:none; }
        QListWidget::item { padding:8px; margin:2px; }
        QListWidget::item:selected { background:#4c78ff; }
        QTableWidget { background:#25262b; color:white; gridline-color:#444; }
        QHeaderView::section { background:#2b2d31; color:white; }
        QStatusBar { background:#2b2d31; color:white; }
        QComboBox { background:#2b2d31; color:white; padding:4px; }
        QMenuBar { background:#2b2d31; color:white; }
        QMenuBar::item:selected { background:#3b3f45; }
        QMenu { background:#25262b; color:white; }
        QMenu::item:selected { background:#4c78ff; }
    )");

    // Add menu bar
    auto* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);
    auto* viewMenu = menuBar->addMenu("View");
    
    auto* toggleMetadataAction = viewMenu->addAction("Show Metadata");
    toggleMetadataAction->setCheckable(true);
    toggleMetadataAction->setChecked(true);
    connect(toggleMetadataAction, &QAction::triggered, this, [this](bool checked) {
        if (metadataDock_) metadataDock_->setVisible(checked);
    });

    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    commandBar_ = toolbar;
    toolbar->setIconSize(QSize(24, 24));
    
    // Open
    auto openAction = toolbar->addAction("Open", this, &MainWindow::openDialog);
    openAction->setShortcut(QKeySequence::Open);
    QIcon openIcon(":/icons/open.svg");
    if (!openIcon.isNull()) openAction->setIcon(openIcon);
    
    // Folder
    auto folderAction = toolbar->addAction("Folder", this, &MainWindow::openFolderDialog);
    QIcon folderIcon(":/icons/folder.svg");
    if (!folderIcon.isNull()) folderAction->setIcon(folderIcon);
    
    // Previous
    auto prevAction = toolbar->addAction("Prev", this, &MainWindow::previousImage);
    prevAction->setShortcut(Qt::Key_Left);
    QIcon prevIcon(":/icons/prev.svg");
    if (!prevIcon.isNull()) prevAction->setIcon(prevIcon);
    
    // Next
    auto nextAction = toolbar->addAction("Next", this, &MainWindow::nextImage);
    nextAction->setShortcut(Qt::Key_Right);
    QIcon nextIcon(":/icons/next.svg");
    if (!nextIcon.isNull()) nextAction->setIcon(nextIcon);
    
    // Fit
    auto fitAction = toolbar->addAction("Fit", this, &MainWindow::fitToWindow);
    QIcon fitIcon(":/icons/fit.svg");
    if (!fitIcon.isNull()) fitAction->setIcon(fitIcon);
    
    // Actual Size
    auto actualAction = toolbar->addAction("1:1", this, &MainWindow::actualSize);
    QIcon actualIcon(":/icons/actual.svg");
    if (!actualIcon.isNull()) actualAction->setIcon(actualIcon);
    
    // Zoom In
    auto zoomInAction = toolbar->addAction("Zoom +", this, &MainWindow::zoomIn);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    QIcon zoomInIcon(":/icons/zoom-in.svg");
    if (!zoomInIcon.isNull()) zoomInAction->setIcon(zoomInIcon);
    
    // Zoom Out
    auto zoomOutAction = toolbar->addAction("Zoom -", this, &MainWindow::zoomOut);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    QIcon zoomOutIcon(":/icons/zoom-out.svg");
    if (!zoomOutIcon.isNull()) zoomOutAction->setIcon(zoomOutIcon);
    
    // Rotate
    auto rotateAction = toolbar->addAction("Rotate", this, &MainWindow::rotateRight);
    QIcon rotateIcon(":/icons/rotate.svg");
    if (!rotateIcon.isNull()) rotateAction->setIcon(rotateIcon);
    
    // Fullscreen
    auto fsAction = toolbar->addAction("Fullscreen", this, &MainWindow::toggleFullscreen);
    fsAction->setShortcut(Qt::Key_F11);
    QIcon fsIcon(":/icons/fullscreen.svg");
    if (!fsIcon.isNull()) fsAction->setIcon(fsIcon);
    
    // Slideshow
    auto slideshowAction = toolbar->addAction("Slideshow", this, &MainWindow::toggleSlideshow);
    QIcon slideshowIcon(":/icons/slideshow.svg");
    if (!slideshowIcon.isNull()) slideshowAction->setIcon(slideshowIcon);
    
    // Bookmark
    auto bookmarkAction = toolbar->addAction("Bookmark", this, &MainWindow::toggleBookmark);
    QIcon bookmarkIcon(":/icons/bookmark.svg");
    if (!bookmarkIcon.isNull()) bookmarkAction->setIcon(bookmarkIcon);
    
    toolbar->addSeparator();

    sortCombo_ = new QComboBox(toolbar);
    sortCombo_->addItems({"Sort: Name", "Sort: Date", "Sort: Size", "Sort: Type"});
    connect(sortCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::applySort);
    toolbar->addWidget(sortCombo_);

    auto* splitter = new QSplitter(this);
    
    // Create thumbnail panel with QScrollArea + GridLayout
    thumbnailScroll_ = new QScrollArea(splitter);
    thumbnailScroll_->setMinimumWidth(330);
    thumbnailScroll_->setMaximumWidth(360);
    thumbnailScroll_->setWidgetResizable(true);
    thumbnailScroll_->setStyleSheet("background-color:#25262b; border:none;");
    
    thumbnailContainer_ = new QWidget();
    thumbnailLayout_ = new QGridLayout(thumbnailContainer_);
    thumbnailLayout_->setSpacing(4);
    thumbnailLayout_->setContentsMargins(4, 4, 4, 4);
    thumbnailLayout_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    thumbnailScroll_->setWidget(thumbnailContainer_);
    
    splitter->addWidget(thumbnailScroll_);
    imageView_ = new QGraphicsView(imageScene_, splitter);
    imageView_->setBackgroundBrush(QColor(0x18, 0x1a, 0x1b));
    imageView_->setRenderHint(QPainter::SmoothPixmapTransform);
    imageView_->setStyleSheet("border:none;");

    splitter->addWidget(imageView_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    metadataTable_ = new QTableWidget(0, 2, this);
    metadataTable_->setHorizontalHeaderLabels({"Key", "Value"});
    metadataTable_->horizontalHeader()->setStretchLastSection(true);
    metadataTable_->verticalHeader()->hide();
    metadataTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    metadataTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    metadataDock_ = new QDockWidget("Metadata", this);
    metadataDock_->setWidget(metadataTable_);
    addDockWidget(Qt::RightDockWidgetArea, metadataDock_);

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
    populateList();

    currentIndex_ = -1;
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (items_[i].isImage()) {
            selectIndex(i);
            return;
        }
    }
    currentImage_ = {};
    imageScene_->clear();
    imageItem_ = nullptr;
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
    updateThumbnailSelection(row);
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
        const QSize viewport = imageView_->viewport()->size();
        scale = std::min(
            viewport.width() / static_cast<double>(image.width()),
            viewport.height() / static_cast<double>(image.height()));
        scale = std::min(scale, 1.0);
    }
    const QSize targetSize(
        std::max(1, static_cast<int>(image.width() * scale)),
        std::max(1, static_cast<int>(image.height() * scale)));
    const QPixmap pixmap = QPixmap::fromImage(image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    
    imageScene_->clear();
    imageItem_ = imageScene_->addPixmap(pixmap);
    
    const QRectF itemRect = imageItem_->boundingRect();
    
    if (fit_) {
        // Fit mode: scene rect = viewport size (no scrolling)
        const QSizeF vpSize = imageView_->viewport()->size();
        imageView_->setSceneRect(0, 0, vpSize.width(), vpSize.height());
        imageItem_->setPos((vpSize.width() - itemRect.width()) / 2.0,
                           (vpSize.height() - itemRect.height()) / 2.0);
    } else {
        // Zoom mode: scene rect large enough for scrolling
        const QSizeF vpSize = imageView_->viewport()->size();
        const QSizeF sceneSize(std::max(itemRect.width(), vpSize.width()),
                                std::max(itemRect.height(), vpSize.height()));
        imageView_->setSceneRect(0, 0, sceneSize.width(), sceneSize.height());
        imageItem_->setPos((sceneSize.width() - itemRect.width()) / 2.0,
                           (sceneSize.height() - itemRect.height()) / 2.0);
        imageView_->resetTransform();
        imageView_->centerOn(imageItem_);
    }
}

void MainWindow::populateList()
{
    // Clear existing thumbnails
    for (auto& pair : thumbnailButtons_) {
        delete pair.second;
    }
    thumbnailButtons_.clear();
    
    const int COLS = 3;
    
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const VfsItem& item = items_[i];
        auto* btn = new QPushButton();
        btn->setIconSize(QSize(90, 90));
        btn->setFixedSize(110, 135);
        btn->setFlat(true);
        btn->setStyleSheet(R"(
            QPushButton {
                border: 1px solid #444;
                border-radius: 4px;
                color: white;
                background-color: #25262b;
                padding: 2px;
            }
            QPushButton:hover {
                background-color: #3b3f45;
            }
        )");
        
        // Set text (filename)
        QString displayName = itemPrefix(item) + "\n" + item.displayName;
        btn->setText(displayName);
        btn->setToolTip(item.uri);
        
        // Connect click signal
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            selectIndex(i);
        });
        
        // Add to grid (3 columns)
        int row = i / COLS;
        int col = i % COLS;
        thumbnailLayout_->addWidget(btn, row, col);
        thumbnailButtons_[i] = btn;
        
        // Load thumbnail asynchronously via updateItemDecoration
        updateItemDecoration(i);
    }
    
    // Add stretch at bottom to push content to top
    int lastRow = static_cast<int>(items_.size()) / COLS + (items_.size() % COLS ? 1 : 0);
    thumbnailLayout_->setRowStretch(lastRow, 1);
}

void MainWindow::updateThumbnailSelection(int row)
{
    for (auto& pair : thumbnailButtons_) {
        if (pair.first == row) {
            pair.second->setStyleSheet(R"(
                QPushButton {
                    border: 2px solid #4c78ff;
                    border-radius: 4px;
                    color: white;
                    background-color: #3b4c8f;
                    padding: 2px;
                }
            )");
        } else {
            pair.second->setStyleSheet(R"(
                QPushButton {
                    border: 1px solid #444;
                    border-radius: 4px;
                    color: white;
                    background-color: #25262b;
                    padding: 2px;
                }
                QPushButton:hover {
                    background-color: #3b3f45;
                }
            )");
        }
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
    if (row < 0 || row >= static_cast<int>(items_.size())) {
        return;
    }
    
    // Find button for this row
    if (thumbnailButtons_.find(row) == thumbnailButtons_.end()) {
        return;
    }
    
    QPushButton* btn = thumbnailButtons_[row];
    const VfsItem& item = items_[row];
    
    // Update text with bookmark indicator
    QString label = itemPrefix(item) + "\n" + item.displayName;
    if (bookmarks_.contains(item.uri)) {
        label = "* " + label;
    }
    btn->setText(label);
    
    // Load and set thumbnail asynchronously
    if (item.isImage()) {
        (void)QtConcurrent::run([this, row, item]() {
            try {
                const QImage thumb = imageLoader_.loadThumbnail(item);
                QMetaObject::invokeMethod(this, [this, row, thumb]() {
                    if (row < 0 || row >= static_cast<int>(items_.size())) {
                        return;
                    }
                    if (thumbnailButtons_.find(row) != thumbnailButtons_.end()) {
                        QPushButton* btn = thumbnailButtons_[row];
                        QPixmap pixmap = QPixmap::fromImage(
                            thumb.scaled(90, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        btn->setIcon(QIcon(pixmap));
                    }
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
            selectIndex(index);
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
    if (isFullScreen()) {
        showNormal();
        if (commandBar_) commandBar_->setVisible(true);
        statusBar()->setVisible(true);
    } else {
        showFullScreen();
        if (commandBar_) commandBar_->setVisible(false);
        statusBar()->setVisible(false);
    }
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
            selectIndex(i);
            return;
        }
    }
}

void MainWindow::lastImage()
{
    for (int i = static_cast<int>(items_.size()) - 1; i >= 0; --i) {
        if (items_[i].isImage()) {
            selectIndex(i);
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
            if (commandBar_) commandBar_->setVisible(true);
            statusBar()->setVisible(true);
            if (metadataDock_) metadataDock_->setVisible(true);
            event->accept();
            return;
        }
        break;
    default:
        break;
    }
    QMainWindow::keyPressEvent(event);
}
