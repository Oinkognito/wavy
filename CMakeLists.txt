cmake_minimum_required(VERSION 3.22)

string(ASCII 27 ESC)
set(RESET "${ESC}[0m")
set(GREEN "${ESC}[32m")  # Success messages
set(YELLOW "${ESC}[33m") # Warning messages
set(BLUE   "${ESC}[34m")  # Information messages

project(Wavy LANGUAGES CXX VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CMake's way of finding Boost since Boost C++ 1.70
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Verbose")

if(CMAKE_BUILD_TYPE STREQUAL "Verbose")
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

option(BUILD_NINJA "Use Ninja to build the project" OFF)

# Check if CMake is using Ninja
if (DEFINED BUILD_NINJA AND BUILD_NINJA)
    message(STATUS "Using ${BLUE}Ninja build system${RESET} for fast parallel builds")
    set(CMAKE_MAKE_PROGRAM ninja)
else()
  message(STATUS "Using ${CMAKE_GENERATOR}...")
endif()

# Option to enable mold linker
option(USE_MOLD "Use mold as the linker" OFF)

if(DEFINED USE_MOLD AND USE_MOLD)
    message(STATUS "Using ${BLUE}mold linker${RESET}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold -Wl,--stats")
    set(CMAKE_LINKER mold)
else()
    message(STATUS "Using default linker")
endif()

# Enable diagnostics on compilation and linking
if (DEFINED COMPILE_REPORT AND COMPILE_REPORT)
 message(STATUS "Requested for compile report.")
 add_compile_options(-ftime-report)
endif()

set(ENCODER_BIN hls_encoder)
set(SERVER_BIN hls_server)
set(DISPATCHER_BIN hls_dispatcher)
set(CLIENT_BIN hls_client)

set(ENCODER_SRC src/encode.cpp)
set(SERVER_SRC src/server.cpp include/server/serverHelper.cpp)
set(DISPATCHER_SRC src/dispatcher.cpp)
set(CLIENT_SRC src/client_receive.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavutil libavformat libavcodec libswresample)
find_package(Boost REQUIRED COMPONENTS log log_setup system thread filesystem date_time regex)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_library(ARCHIVE_LIB archive)
if(ARCHIVE_LIB)
    message(STATUS "Found archive library: ${ARCHIVE_LIB}")
else()
    message(WARNING "Archive library not found")
endif()

find_library(ZSTD_LIB libzstd)
pkg_check_modules(ZSTD REQUIRED libzstd)

if(ZSTD_FOUND)
    message(STATUS "pkg-config found Zstd: ${ZSTD_LIBRARIES}")
    message(STATUS "Zstd include directories: ${ZSTD_INCLUDE_DIRS}")
else()
    message(WARNING "pkg-config could not find Zstd")
endif()

function(add_custom_executable name src_file)
    add_executable(${name} ${src_file})
    target_include_directories(${name} PRIVATE ${FFMPEG_INCLUDE_DIRS} ${ZSTD_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE ${FFMPEG_LIBRARIES} OpenSSL::Crypto ${ZSTD_LIBRARIES})
endfunction()

add_custom_executable(${ENCODER_BIN} ${ENCODER_SRC})
target_link_libraries(${ENCODER_BIN} PRIVATE Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem Boost::date_time Boost::regex Threads::Threads)


add_executable(${CLIENT_BIN} ${CLIENT_SRC})
message(STATUS "<< Client compilation with std-C++20... >>")
target_compile_features(${CLIENT_BIN} PRIVATE cxx_std_20)
target_link_libraries(${CLIENT_BIN} PRIVATE Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem Boost::date_time Boost::regex Threads::Threads OpenSSL::SSL OpenSSL::Crypto ${FFMPEG_LIBRARIES} ${ARCHIVE_LIB} ${ZSTD_LIBRARIES})

add_library(wavy-server SHARED include/server/serverHelper.cpp)

# Specify include directories
target_include_directories(wavy-server PRIVATE ${ZSTD_INCLUDE_DIRS})

# Link required libraries (PUBLIC as we need these symbols to be read by server.cpp at runtime)
target_link_libraries(wavy-server PUBLIC Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem OpenSSL::SSL OpenSSL::Crypto ${ARCHIVE_LIB} ${ZSTD_LIBRARIES})

# Now, link the shared library to the server executable
add_executable(${SERVER_BIN} src/server.cpp)

target_compile_features(${SERVER_BIN} PRIVATE cxx_std_20)
target_link_libraries(${SERVER_BIN} PRIVATE  ${ARCHIVE_LIB} ${ZSTD_LIBRARIES} wavy-server)

add_executable(${DISPATCHER_BIN} ${DISPATCHER_SRC})
target_link_libraries(${DISPATCHER_BIN} PRIVATE Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem Boost::date_time Boost::regex Threads::Threads OpenSSL::SSL OpenSSL::Crypto ${ARCHIVE_LIB} ${ZSTD_LIBRARIES})

add_custom_target(format COMMAND clang-format -i src/*.cpp include/*.h)
add_custom_target(tidy COMMAND clang-tidy -fix src/*.cpp include/*.h --)

add_custom_target(remove COMMAND rm -f *.ts *.m3u8)

# Individual build targets
add_custom_target(build_encoder COMMAND ${CMAKE_COMMAND} --build . --target ${ENCODER_BIN})
add_custom_target(build_decoder COMMAND ${CMAKE_COMMAND} --build . --target ${DECODER_BIN})
add_custom_target(build_playback COMMAND ${CMAKE_COMMAND} --build . --target ${PLAYBACK_BIN})
add_custom_target(build_server COMMAND ${CMAKE_COMMAND} --build . --target ${SERVER_BIN})
add_custom_target(build_dispatcher COMMAND ${CMAKE_COMMAND} --build . --target ${DISPATCHER_BIN})
add_custom_target(build_client COMMAND ${CMAKE_COMMAND} --build . --target ${CLIENT_BIN})

if(DEFINED BUILD_TARGET)
    set(TARGET_BUILT ${BUILD_TARGET})
else()
    set(TARGET_BUILT "All")
endif()

include(CheckIncludeFiles)
include(CheckIncludeFileCXX)

set(STANDARD_HEADERS
    "assert.h"
    "ctype.h"
    "errno.h"
    "fcntl.h"
    "float.h"
    "inttypes.h"
    "limits.h"
    "math.h"
    "setjmp.h"
    "signal.h"
    "stdarg.h"
    "stddef.h"
    "stdio.h"
    "stdlib.h"
    "string.h"
    "time.h"
    "unistd.h"
    "stdbool.h"
)

set(STANDARD_CXX_HEADERS
    "iostream"
    "fstream"
    "sstream"
    "vector"
    "array"
    "string"
    "map"
    "unordered_map"
    "set"
    "unordered_set"
    "algorithm"
    "numeric"
    "iterator"
    "memory"
    "functional"
    "type_traits"
    "limits"
    "cmath"
    "exception"
    "stdexcept"
    "thread"
    "mutex"
    "future"
    "condition_variable"
    "chrono"
    "random"
)

message(STATUS "${BLUE}Checking Standard C Headers...${RESET}")
foreach(header ${STANDARD_HEADERS})
    check_include_files(${header} HAS_${header})
    if(HAS_${header})
      message(STATUS "${GREEN}✔ Found header: ${header}${RESET}")
    else()
      message(WARNING "${YELLOW}✘ Missing header: ${header}${RESET}")
    endif()
endforeach()

message(STATUS "${BLUE}Checking Standard C++ Headers...${RESET}")
foreach(header ${STANDARD_CXX_HEADERS})
    check_include_file_cxx(${header} HAS_${header})
    if(HAS_${header})
      message(STATUS "${GREEN}✔ Found header: ${header}${RESET}")
    else()
      message(WARNING "${YELLOW}✘ Missing header: ${header}${RESET}")
    endif()
endforeach()

message(STATUS "\n\n━━━━━━━━━━━━━━━━━ ${PROJECT_NAME} project v${PROJECT_VERSION} ━━━━━━━━━━━━━━━━━\n")
message(STATUS "┌─ Build Configuration for ${PROJECT_NAME} ────────────────────────")
message(STATUS "│ Operating System        : ${CMAKE_SYSTEM_NAME}")
message(STATUS "│ Build Type              : ${CMAKE_BUILD_TYPE}")
message(STATUS "│ Build System            : ${CMAKE_MAKE_PROGRAM}")
message(STATUS "│ Target(s)               : ${TARGET_BUILT}")
message(STATUS "│ CMake Version           : ${CMAKE_VERSION}")
message(STATUS "│ Compiler                : ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "│ Compiler Flags          : ${CMAKE_CXX_FLAGS}")
message(STATUS "│ Executable Names        : ${ENCODER_BIN}, ${CLIENT_BIN}, ${SERVER_BIN}, ${DISPATCHER_BIN}")
message(STATUS "│ C++ Standard            : ${CMAKE_CXX_STANDARD}")
message(STATUS "│ FFmpeg Libraries        : ${FFMPEG_LIBRARIES}")
message(STATUS "│ Boost Libraries         : Boost::log, Boost::system, Boost::thread, etc.")
message(STATUS "│ OpenSSL                 : ${OPENSSL_LIBRARIES}")
message(STATUS "│ ZSTD                    : ${ZSTD_LIBRARIES}")
message(STATUS "│ Archive Library         : ${ARCHIVE_LIB}")
message(STATUS "│ Linker Executable       : ${CMAKE_LINKER}")
message(STATUS "│ Linker Flags            : ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "└──────────────────────────────────────────────────────────\n")
