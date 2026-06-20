#include "MainWindow.h"

#include "core/ThumbnailCache.h"
#include "core/Vfs.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--smoke-test") {
            QCoreApplication coreApp(argc, argv);
            Vfs vfs;
            const auto items = vfs.openCollection(QCoreApplication::applicationDirPath());
            QTextStream(stdout) << "DongImageViewer smoke test ok: " << items.size() << " browsable items\n";
            return 0;
        }
        if (arg == "--rebuild-cache") {
            QCoreApplication coreApp(argc, argv);
            ThumbnailCache cache;
            cache.rebuild();
            QTextStream(stdout) << "DongImageViewer thumbnail cache rebuilt\n";
            return 0;
        }
    }

    QApplication app(argc, argv);

    QString initialTarget;
    const QStringList args = app.arguments();
    if (args.size() > 1) {
        initialTarget = args.at(1);
        if (initialTarget.startsWith("dongviewer://")) {
            const QUrl url(initialTarget);
            const QUrlQuery query(url);
            initialTarget = query.queryItemValue("path");
        }
    }

    MainWindow window(initialTarget);
    window.show();
    return app.exec();
}
