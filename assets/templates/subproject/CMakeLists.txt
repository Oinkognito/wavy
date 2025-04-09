cmake_minimum_required(VERSION 3.22)
project(example_subdir_proj LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES main.cpp)

# CMake's way of finding Boost since Boost C++ 1.70
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Find Boost (this is probably needed throughout Wavy so this is a must)
find_package(Boost REQUIRED COMPONENTS log log_setup system thread)

find_package(PkgConfig REQUIRED)

# 
# TO FIND FFMPEG
# 
# pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
#    libavformat
#    libavutil
#    libswresample
#    libavcodec
#)

# Executable
add_executable(example_subdir_proj ${SOURCES})

# Include directories
target_include_directories(example_subdir_proj PRIVATE ${Boost_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})

# Link Libraries
target_link_libraries(example_subdir_proj PRIVATE 
    Boost::log Boost::log_setup Boost::system Boost::thread
    # PkgConfig::LIBAV 
)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(example_subdir_proj PRIVATE BOOST_LOG_DLL)
