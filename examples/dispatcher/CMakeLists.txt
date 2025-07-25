cmake_minimum_required(VERSION 3.22)
project(example_dispatcher LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(DISPATCH_SRC main.cpp)

set(INDICATORS_HEADERS ../../external/indicators/include)

# Enable Boost C++ 1.70+ policy
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# find libarchive
find_library(ARCHIVE_LIB_DISPATCHER archive)

# Find ZSTD libraries
find_package(PkgConfig REQUIRED)

find_library(ZSTD_LIB libzstd)
pkg_check_modules(ZSTD REQUIRED libzstd)

# Executables
add_executable(example_dispatcher ${DISPATCH_SRC})

# Include directories
target_include_directories(example_dispatcher PRIVATE ${ZSTD_INCLUDE_DIRS} ${INDICATORS_HEADERS} ${CMAKE_SOURCE_DIR})

# Link Libraries
target_link_libraries(example_dispatcher PRIVATE
    wavy-logger
    ${ARCHIVE_LIB_DISPATCHER}
    OpenSSL::SSL
    ${ZSTD_LIBRARIES}
)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(example_dispatcher PRIVATE BOOST_LOG_DLL)
