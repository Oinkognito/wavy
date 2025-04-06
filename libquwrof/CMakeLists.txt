cmake_minimum_required(VERSION 3.22)
project(quwrof)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    window/src/MainWindow.cpp
)

set(HEADERS
    window/entry.hpp
)

set(RESOURCES
    ../resources/resources.qrc
)

add_library(quwrof SHARED
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_include_directories(quwrof PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

target_link_libraries(quwrof PUBLIC Qt6::Widgets)

set(RESOURCES ${RESOURCES} PARENT_SCOPE) # expose all resources to Wavy-UI
