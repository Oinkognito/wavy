cmake_minimum_required(VERSION 3.22)
project(example_flac_metadata LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES main.cpp)

# Include the FLAC metadata headers
include_directories(${CMAKE_SOURCE_DIR}/../../codecs/flac)

# Enable Boost C++ 1.70+ policy
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Find FLAC++
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac++)

# Executable
add_executable(example_flac_metadata ${SOURCES})

# Include directories
target_include_directories(example_flac_metadata PRIVATE ${CMAKE_SOURCE_DIR})

# Link Libraries
target_link_libraries(example_flac_metadata PRIVATE
    wavy-logger
    PkgConfig::FLAC
    pthread
)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(example_flac_metadata PRIVATE BOOST_LOG_DLL)
