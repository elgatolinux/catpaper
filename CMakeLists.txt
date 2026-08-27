cmake_minimum_required(VERSION 3.21)
project(catpaper VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Qml Quick Multimedia Network)
find_package(LayerShellQt CONFIG QUIET)

qt_standard_project_setup()

qt_add_executable(catpaper
    src/main.cpp
    src/Paths.h       src/Paths.cpp
    src/PywalTheme.h  src/PywalTheme.cpp
    src/PersistentSettings.h src/PersistentSettings.cpp
    src/Search.h      src/Search.cpp
    src/Thumbs.h      src/Thumbs.cpp
    src/Backend.h     src/Backend.cpp
    resources.qrc
)

target_link_libraries(catpaper PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick
    Qt6::Multimedia Qt6::Network
)

target_compile_definitions(catpaper PRIVATE
    CATPAPER_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)

if(LayerShellQt_FOUND)
    message(STATUS "layer-shell-qt encontrado: overlay wlr-layer-shell habilitado")
    target_link_libraries(catpaper PRIVATE LayerShellQt::Interface)
    target_compile_definitions(catpaper PRIVATE CATPAPER_LAYER_SHELL)
endif()

install(TARGETS catpaper DESTINATION bin)
