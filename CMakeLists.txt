cmake_minimum_required(VERSION 3.7)

if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
    # Standalone build, call project and look for PlotJuggler
    project(plotjuggler_can_plugins)

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(PlotJuggler REQUIRED)
else()
    # Included from project (assuming from PlotJuggler)
    # Set PlotJuggler_LIBRARY manually since find_package fails in this configuration.
    set(PlotJuggler_LIBRARY plotjuggler_base)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(APPLE AND EXISTS /usr/local/opt/qt5)
    # Homebrew installs Qt5 (up to at least 5.9.1) in
    # /usr/local/qt5, ensure it can be found by CMake since
    # it is not in the default /usr/local prefix.
    # source: https://github.com/Homebrew/homebrew-core/issues/8392#issuecomment-325226494
    list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt5")
    set(CMAKE_MACOSX_RPATH 1)
endif()

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)

find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Concurrent
    Xml
    Svg
)

add_subdirectory(3rdparty/dbcppp)

find_package(PlotJuggler REQUIRED)

include_directories(
    ${Qt5Core_INCLUDE_DIRS}
    ${Qt5Widgets_INCLUDE_DIRS}
    ${Qt5Concurrent_INCLUDE_DIRS}
    ${Qt5Xml_INCLUDE_DIRS}
    ${PlotJuggler_INCLUDE_DIRS}
    ${Qt5Svg_INCLUDE_DIRS}
    3rdparty/dbcppp/include
)

set( LIBRARIES
    Qt5::Core
    Qt5::Widgets
    Qt5::Xml
    Qt5::Concurrent
    Qt5::Svg
    ${PlotJuggler_LIBRARY}
    libdbcppp
    )

add_library(CanFrameProcessor STATIC
    PluginsCommonCAN/CanFrameProcessor.cpp
    PluginsCommonCAN/N2kMsg/GenericFastPacket.c
    PluginsCommonCAN/select_can_database.h
    PluginsCommonCAN/select_can_database.cpp
)
target_link_libraries(CanFrameProcessor
    ${LIBRARIES}
    libdbcppp
)
set_property(TARGET CanFrameProcessor PROPERTY POSITION_INDEPENDENT_CODE ON)

if(${Qt5Widgets_FOUND})
    message("-- Found Qt5, building DataLoadCAN.")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/DataLoadCAN)
else()
    message("-- Since Qt5 not found, build without DataLoadCAN.")
endif()

find_package(Qt5 OPTIONAL_COMPONENTS
    SerialBus
)
# Build&Install DataStreamCAN only if SerialBus found.
if(${Qt5SerialBus_FOUND})
    message("-- Found Qt5::SerialBus, building DataStreamCAN.")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/DataStreamCAN)
else()
    message("-- Since Qt5::SerialBus not found, cannot build DataStreamCAN.")
endif()
