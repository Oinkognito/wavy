cmake_minimum_required(VERSION 3.22)
project(transcode LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(ENCODE_MP3_SRC transcode.cpp)
set(ENCODE_FLAC_SRC transcode-flac.cpp)

# Enable Boost C++ 1.70+ policy
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

# Find FLAC++
pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac++)

# Executables
add_executable(transcode_mp3 ${ENCODE_MP3_SRC})
add_executable(transcode_flac ${ENCODE_FLAC_SRC})

# Include directories
target_include_directories(transcode_mp3 PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(transcode_flac PRIVATE ${Boost_INCLUDE_DIRS})

# Link Libraries
target_link_libraries(transcode_mp3 PRIVATE 
    Boost::log Boost::log_setup Boost::system Boost::thread 
    PkgConfig::LIBAV
)

target_link_libraries(transcode_flac PRIVATE 
    PkgConfig::LIBAV 
    PkgConfig::FLAC
)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(transcode_mp3 PRIVATE BOOST_LOG_DLL)
