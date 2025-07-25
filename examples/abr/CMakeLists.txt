cmake_minimum_required(VERSION 3.22)
project(example_abr LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES main.cpp)

# CMake's way of finding Boost since Boost C++ 1.70
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Find Boost
find_package(OpenSSL REQUIRED)

# Find FFmpeg libraries
find_package(PkgConfig REQUIRED)

# Executable
add_executable(example_abr ${SOURCES})

# Include directories
target_include_directories(example_abr PRIVATE ${CMAKE_SOURCE_DIR})

# Link Libraries
target_link_libraries(example_abr PRIVATE
    wavy-logger
    OpenSSL::SSL OpenSSL::Crypto
)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(example_abr PRIVATE BOOST_LOG_DLL)
