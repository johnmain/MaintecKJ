#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtCore/QUrl>
#include <QDebug>
#include <QDir>
#include "SingerModel.h"
#include "SongQueueModel.h"
#include "DatabaseManager.h"
#include "FolderScanner.h"
#include "SongDatabaseModel.h"
#include "MediaPlayerController.h"
#include "RotationController.h"
#include "CdgRenderer.h"
#include "MidiController.h"

// Supplied by CMake (see target_compile_definitions in CMakeLists.txt). Kept
// optional so the file still compiles if the definition is ever removed.
#ifndef MAINTECKJ_QML_DIR
#define MAINTECKJ_QML_DIR ""
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("MaintecKJ"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("mainteckj.local"));
    QCoreApplication::setApplicationName(QStringLiteral("MaintecKJ"));
    
    qDebug() << "Starting application...";
    
    // Create C++ model instances (database first so models can restore state)
    DatabaseManager databaseManager;
    SingerModel singerModel;
    SongQueueModel songQueueModel;
    FolderScanner folderScanner;
    SongDatabaseModel songDatabaseModel(&databaseManager);
    MediaPlayerController mediaPlayer;
    RotationController rotation;
    MidiController midiController;

    rotation.setSingerModel(&singerModel);
    rotation.setQueueModel(&songQueueModel);
    rotation.setPlayer(&mediaPlayer);

    // DJ controller: left fader drives the tempo, right fader drives the key
    // shift (live, and recorded against the singer's queue row), right play
    // button toggles playback.
    midiController.setPlayer(&mediaPlayer);
    midiController.setQueueModel(&songQueueModel);

    // A song ending on its own advances the rotation to the next singer.
    QObject::connect(&mediaPlayer, &MediaPlayerController::songFinished,
                     &rotation, &RotationController::advance);

    // Restore persisted singer rotation + song queue
    singerModel.setDatabaseManager(&databaseManager);
    songQueueModel.setDatabaseManager(&databaseManager);
    
    QQmlApplicationEngine engine;
    
    // Register C++ types for QML
    qmlRegisterType<SingerModel>("MaintecKJ.Models", 1, 0, "SingerModel");
    qmlRegisterType<SongQueueModel>("MaintecKJ.Models", 1, 0, "SongQueueModel");
    qmlRegisterType<CdgRenderer>("MaintecKJ", 1, 0, "CdgRenderer");
    
    // Expose model instances to QML as properties
    engine.rootContext()->setContextProperty("singerModel", &singerModel);
    engine.rootContext()->setContextProperty("songQueueModel", &songQueueModel);
    engine.rootContext()->setContextProperty("databaseManager", &databaseManager);
    engine.rootContext()->setContextProperty("folderScanner", &folderScanner);
    engine.rootContext()->setContextProperty("songDatabaseModel", &songDatabaseModel);
    engine.rootContext()->setContextProperty("mediaPlayer", &mediaPlayer);
    engine.rootContext()->setContextProperty("rotationController", &rotation);
    engine.rootContext()->setContextProperty("midiController", &midiController);
    
    // Resolve the generated MaintecKJ QML module (it is emitted next to the
    // executable) and the QML sources without baking in absolute paths, so the
    // app runs from any checkout or install prefix.
    engine.addImportPath(QCoreApplication::applicationDirPath());

    QString qmlDir = QString::fromUtf8(MAINTECKJ_QML_DIR);
    if (qmlDir.isEmpty())
        qmlDir = QCoreApplication::applicationDirPath() + QStringLiteral("/qml");

    const QUrl url = QUrl::fromLocalFile(qmlDir + QStringLiteral("/Main.qml"));
    qDebug() << "Loading QML from:" << url;
    qDebug() << "QML import paths:" << engine.importPathList();
    
    engine.load(url);
    
    return app.exec();
}
