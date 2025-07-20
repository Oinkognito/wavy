cmake_minimum_required(VERSION 3.22)
project(example_hls_seg LANGUAGES CXX)

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
find_package(Boost REQUIRED COMPONENTS log log_setup system thread)

# Find FFmpeg libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavformat
    libavutil
    libswresample
    libavcodec
)

# Executable
add_executable(example_hls_seg ${SOURCES})

# Include directories
target_include_directories(example_hls_seg PRIVATE ${Boost_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})

# Link Libraries
target_link_libraries(example_hls_seg PRIVATE wavy-ffmpeg
    Boost::log Boost::log_setup Boost::system Boost::thread 
    PkgConfig::LIBAV
)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(example_hls_seg PRIVATE BOOST_LOG_DLL)
