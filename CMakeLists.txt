cmake_minimum_required(VERSION 3.16)
project(MaintecKJ VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql Multimedia Concurrent)

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
        # These panels are loaded by URL at runtime rather than imported, but they
        # are listed so the deployment step can see which QML modules the app
        # actually needs - QtMultimedia and QtQuick.Dialogs in particular, which
        # nothing else would reveal.
        qml/components/BackgroundLibraryPanel.qml
        qml/components/BackgroundPlaylistPanel.qml
        qml/components/DeckPanel.qml
        qml/components/SingerPanel.qml
        qml/components/SingerQueuePanel.qml
        qml/components/SongDatabasePanel.qml
)

target_link_libraries(mainteckj-app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Multimedia
    Qt6::Concurrent
    PkgConfig::RUBBERBAND
    PkgConfig::ALSA
)

# The QML is loaded from disk. A development build is pointed straight at the
# source tree so edits show up without a rebuild; a packaging build must not do
# that, or an installed copy would still be reading the machine it was built on.
# Release builds fall back to the QML installed beside the binary.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(mainteckj-app PRIVATE
        MAINTECKJ_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    )
endif()

install(TARGETS mainteckj-app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# The QML is loaded from disk rather than the resource, so it travels alongside.
# main.cpp looks for it at ../share/mainteckj/qml relative to the binary.
install(DIRECTORY qml/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mainteckj/qml
    FILES_MATCHING PATTERN "*.qml"
)

# A short readme and a launcher, so the unpacked tree is usable on its own.
install(FILES packaging/README.md DESTINATION .)
install(PROGRAMS packaging/run.sh DESTINATION .)

# The deploy step only copies the platform plugins it can detect as needed, which
# here is xcb. The offscreen plugin is added explicitly: it costs half a megabyte
# and is what allows the packaged app to be smoke-tested on a machine with no
# display attached.
install(FILES "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt6/plugins/platforms
)

# Pulls in the Qt runtime: the libraries the app links, the platform,
# imageformat, sql and multimedia plugins, and - because this is the QML-aware
# variant, which runs qmlimportscanner - every QML module the panels import.
# The plain qt_generate_deploy_app_script does not do the last part, which
# leaves the app unable to import QtQuick at all once deployed.
qt_generate_deploy_qml_app_script(
    TARGET mainteckj-app
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# `cpack` then produces MaintecKJ-<version>-linux-x86_64.tar.gz containing bin/,
# lib/ and share/ - unpack it anywhere and run bin/mainteckj-app.
set(CPACK_PACKAGE_NAME "MaintecKJ")
set(CPACK_PACKAGE_VENDOR "MaintecKJ")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Karaoke host system with background music mode")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "MaintecKJ-${PROJECT_VERSION}-linux-x86_64")
set(CPACK_GENERATOR "TGZ")
set(CPACK_STRIP_FILES TRUE)
include(CPack)
