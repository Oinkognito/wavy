cmake_minimum_required(VERSION 3.22)
project(Wavy-UI)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Add libquwrof
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libquwrof ${CMAKE_BINARY_DIR}/libquwrof)

add_executable(Wavy-UI main.cpp ${RESOURCES})

target_link_libraries(Wavy-UI PRIVATE quwrof)
