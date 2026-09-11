#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtCore/QUrl>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <utility>
#include "SingerModel.h"
#include "SongQueueModel.h"
#include "DatabaseManager.h"
#include "FolderScanner.h"
#include "SongDatabaseModel.h"
#include "MediaPlayerController.h"
#include "RotationController.h"
#include "CdgRenderer.h"
#include "MidiController.h"
#include "OpenKjImporter.h"
#include "BackgroundPlaylistModel.h"
#include "ModeController.h"
#include "SongListExporter.h"

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
    // Same model class, pointed at the background collection instead.
    SongDatabaseModel backgroundSongModel(&databaseManager, true);
    BackgroundPlaylistModel backgroundPlaylistModel;
    MediaPlayerController mediaPlayer;
    RotationController rotation;
    MidiController midiController;
    OpenKjImporter openKjImporter;
    SongListExporter songListExporter;
    ModeController modeController;

    rotation.setSingerModel(&singerModel);
    rotation.setQueueModel(&songQueueModel);
    rotation.setPlayer(&mediaPlayer);

    // DJ controller: left fader drives the tempo, right fader drives the key
    // shift (live, and recorded against the singer's queue row), right play
    // button toggles playback.
    midiController.setPlayer(&mediaPlayer);
    midiController.setQueueModel(&songQueueModel);

    // Import regular singers and their song history from an OpenKJ export.
    openKjImporter.setDatabaseManager(&databaseManager);
    openKjImporter.setSingerModel(&singerModel);
    openKjImporter.setSongQueueModel(&songQueueModel);

    // Plain Artist/Title list of the whole library, for the song-book workflow.
    songListExporter.setDatabaseManager(&databaseManager);

    // A song ending on its own is routed by the mode: karaoke advances the
    // rotation (without auto-playing), background music plays the next entry.
    modeController.setPlayer(&mediaPlayer);
    modeController.setRotation(&rotation);
    modeController.setBackgroundPlaylist(&backgroundPlaylistModel);

    QObject::connect(&mediaPlayer, &MediaPlayerController::songFinished,
                     &modeController, &ModeController::onSongFinished);

    // Restore persisted singer rotation + song queue
    singerModel.setDatabaseManager(&databaseManager);
    songQueueModel.setDatabaseManager(&databaseManager);
    backgroundPlaylistModel.setDatabaseManager(&databaseManager);
    
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
    engine.rootContext()->setContextProperty("backgroundSongModel", &backgroundSongModel);
    engine.rootContext()->setContextProperty("backgroundPlaylistModel", &backgroundPlaylistModel);
    engine.rootContext()->setContextProperty("modeController", &modeController);
    engine.rootContext()->setContextProperty("mediaPlayer", &mediaPlayer);
    engine.rootContext()->setContextProperty("rotationController", &rotation);
    engine.rootContext()->setContextProperty("midiController", &midiController);
    engine.rootContext()->setContextProperty("openKjImporter", &openKjImporter);
    engine.rootContext()->setContextProperty("songListExporter", &songListExporter);
    
    // Pick up the mode the app was left in. Deliberately not setMode(): there is
    // no deck to fade out at start-up.
    modeController.restore();

    // Resolve the generated MaintecKJ QML module (it is emitted next to the
    // executable) and the QML sources without baking in absolute paths, so the
    // app runs from any checkout, install prefix or relocated bundle.
    const QString appDir = QCoreApplication::applicationDirPath();

    // A deployed copy keeps the QML beside the binary or under share/; a build
    // tree only has the source directory, which CMake passes in.
    QStringList candidates;
    const QString fromEnvironment = qEnvironmentVariable("MAINTECKJ_QML_DIR");
    if (!fromEnvironment.isEmpty())
        candidates << fromEnvironment;
    const QString fromBuild = QString::fromUtf8(MAINTECKJ_QML_DIR);
    if (!fromBuild.isEmpty())
        candidates << fromBuild;
    candidates << appDir + QStringLiteral("/qml")
               << appDir + QStringLiteral("/../share/mainteckj/qml")
               << appDir + QStringLiteral("/../lib/mainteckj/qml");

    QString qmlDir;
    for (const QString &candidate : std::as_const(candidates)) {
        const QString normalised = QDir::cleanPath(candidate);
        if (QFile::exists(normalised + QStringLiteral("/Main.qml"))) {
            qmlDir = normalised;
            break;
        }
    }

    if (qmlDir.isEmpty()) {
        qCritical() << "Could not find Main.qml. Looked in:" << candidates;
        return 1;
    }

    engine.addImportPath(appDir);

    const QUrl url = QUrl::fromLocalFile(qmlDir + QStringLiteral("/Main.qml"));
    qDebug() << "Loading QML from:" << url;
    qDebug() << "QML import paths:" << engine.importPathList();
    
    engine.load(url);
    
    return app.exec();
}
