cmake_minimum_required(VERSION 3.22)
project(make_hls LANGUAGES CXX)

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
add_executable(make_hls ${SOURCES})

# Include directories
target_include_directories(make_hls PRIVATE ${Boost_INCLUDE_DIRS})

# Link Libraries
target_link_libraries(make_hls PRIVATE 
    Boost::log Boost::log_setup Boost::system Boost::thread 
    PkgConfig::LIBAV
)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(make_hls PRIVATE BOOST_LOG_DLL)
