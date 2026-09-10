#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtCore/QUrl>
#include <QDebug>
#include <QDir>
#include "SingerModel.h"
#include "SongQueueModel.h"
#include "DatabaseManager.h"
#include "FolderScanner.h"
#include "SongDatabaseModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    qDebug() << "Starting application...";
    
    // Create C++ model instances
    SingerModel singerModel;
    SongQueueModel songQueueModel;
    DatabaseManager databaseManager;
    FolderScanner folderScanner;
    SongDatabaseModel songDatabaseModel(&databaseManager);
    
    QQmlApplicationEngine engine;
    
    // Register C++ types for QML
    qmlRegisterType<SingerModel>("MaintecKJ.Models", 1, 0, "SingerModel");
    qmlRegisterType<SongQueueModel>("MaintecKJ.Models", 1, 0, "SongQueueModel");
    
    // Expose model instances to QML as properties
    engine.rootContext()->setContextProperty("singerModel", &singerModel);
    engine.rootContext()->setContextProperty("songQueueModel", &songQueueModel);
    engine.rootContext()->setContextProperty("databaseManager", &databaseManager);
    engine.rootContext()->setContextProperty("folderScanner", &folderScanner);
    engine.rootContext()->setContextProperty("songDatabaseModel", &songDatabaseModel);
    
    // Add the build directory to import path
    QDir buildDir(QString::fromUtf8("/home/jmain/Develop/MaintecKJ/build"));
    engine.addImportPath(buildDir.absolutePath());
    
    // Load the QML file
    const QUrl url(QStringLiteral("file:///home/jmain/Develop/MaintecKJ/qml/Main.qml"));
    qDebug() << "Loading QML from:" << url;
    qDebug() << "QML import paths:" << engine.importPathList();
    
    engine.load(url);
    
    return app.exec();
}
