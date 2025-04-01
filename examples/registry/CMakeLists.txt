cmake_minimum_required(VERSION 3.22)
project(registry_create LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES main.cpp)

# Find FFmpeg libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavutil
    libavcodec
    libavformat
)


# Executable
add_executable(registry-create ${SOURCES})

target_include_directories(registry-create PRIVATE ${CMAKE_SOURCE_DIR})

# Link Libraries
target_link_libraries(registry-create PRIVATE PkgConfig::LIBAV)
