cmake_minimum_required(VERSION 3.22)
project(Wavy-UI)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(Wavy-UI main.cpp MainWindow.cpp ../resources/resources.qrc)
target_link_libraries(Wavy-UI Qt6::Widgets)
