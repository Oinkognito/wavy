cmake_minimum_required(VERSION 3.22)
project(example_m3u8_parser LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES main.cpp)

# CMake's way of finding Boost since Boost C++ 1.70
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Find FFmpeg libraries
find_package(PkgConfig REQUIRED)

# Executable
add_executable(example_m3u8_parser ${SOURCES})

# Include directories
target_include_directories(example_m3u8_parser PRIVATE ${CMAKE_SOURCE_DIR})

# Link Libraries
target_link_libraries(example_m3u8_parser PRIVATE wavy-logger)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(example_m3u8_parser PRIVATE BOOST_LOG_DLL)
