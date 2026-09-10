cmake_minimum_required(VERSION 3.16)
project(MaintecKJ VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql Multimedia)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RUBBERBAND REQUIRED IMPORTED_TARGET rubberband)
# MIDI input for the DJ controller. Qt 6 has no MIDI module, so the ALSA
# sequencer is driven directly.
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)

qt_standard_project_setup()

qt_add_executable(mainteckj-app
    src/main.cpp
    src/SingerModel.cpp
    src/SingerModel.h
    src/SongQueueModel.cpp
    src/SongQueueModel.h
    src/RotationController.cpp
    src/RotationController.h
    src/OpenKjImporter.cpp
    src/OpenKjImporter.h
    src/SongListExporter.cpp
    src/SongListExporter.h
    src/BackgroundPlaylistModel.cpp
    src/BackgroundPlaylistModel.h
    src/ModeController.cpp
    src/ModeController.h
    src/DatabaseManager.cpp
    src/DatabaseManager.h
    src/FolderScanner.cpp
    src/FolderScanner.h
    src/SongDatabaseModel.cpp
    src/SongDatabaseModel.h
    src/MediaPlayerController.cpp
    src/MediaPlayerController.h
    src/RubberBandAudioEngine.cpp
    src/RubberBandAudioEngine.h
    src/CdgRenderer.cpp
    src/CdgRenderer.h
    src/MidiController.cpp
    src/MidiController.h
)

qt_add_qml_module(MaintecKJ
    URI MaintecKJ
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(mainteckj-app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Multimedia
    PkgConfig::RUBBERBAND
    PkgConfig::ALSA
)

# The QML UI is loaded from the source tree at runtime. Hand the location to the
# app through the build system so it is not baked into main.cpp as an absolute
# path that only works on one machine.
target_compile_definitions(mainteckj-app PRIVATE
    MAINTECKJ_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)

install(TARGETS mainteckj-app RUNTIME DESTINATION bin)
