cmake_minimum_required(VERSION 3.16)
project(MaintecKJ VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql)

qt_standard_project_setup()

qt_add_executable(mainteckj-app
    src/main.cpp
    src/SingerModel.cpp
    src/SingerModel.h
    src/SongQueueModel.cpp
    src/SongQueueModel.h
    src/DatabaseManager.cpp
    src/DatabaseManager.h
    src/FolderScanner.cpp
    src/FolderScanner.h
    src/SongDatabaseModel.cpp
    src/SongDatabaseModel.h
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
)

install(TARGETS mainteckj-app RUNTIME DESTINATION bin)
