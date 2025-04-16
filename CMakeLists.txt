cmake_minimum_required(VERSION 3.22)

string(ASCII 27 ESC)
set(RESET "${ESC}[0m")
set(GREEN "${ESC}[32m")  # Success messages
set(YELLOW "${ESC}[33m") # Warning messages
set(BLUE   "${ESC}[34m")  # Information messages
set(PURPLE "${ESC}[35m")
set(BOLD "${ESC}[1m")
set(UNDERLINE "${ESC}[4m")

project(Wavy LANGUAGES C CXX VERSION 1.0)

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
option(BUILD_EXAMPLES "Compile examples for the project" OFF)
option(BUILD_UI "Build Wavy-UI using libquwrof" OFF)
option(AUTOGEN_HEADER "Autogenerate Config Header for the project" ON)

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

set(OWNER_BIN wavy_owner)
set(SERVER_BIN wavy_server)
set(CLIENT_BIN wavy_client)

set(OWNER_SRC src/Owner.cpp)
set(SERVER_SRC src/Server.cpp libwavy/server/serverHelper.cpp)
set(CLIENT_SRC src/Client.cpp)

find_package(PkgConfig REQUIRED)
if (DEFINED NO_FFMPEG AND NO_FFMPEG)
  message(STATUS "Skipping FFmpeg check...")
else ()
  pkg_check_modules(FFMPEG REQUIRED libavutil libavformat libavcodec libswresample)
endif()
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


if (DEFINED NO_TBB AND NO_TBB)
  message(STATUS "Skippping module Intel-oneTBB...")
else()
  find_package(TBB REQUIRED tbb)
  pkg_check_modules(TBB REQUIRED tbb)
endif()

function(add_custom_executable name src_file)
    add_executable(${name} ${src_file})
    target_include_directories(${name} PRIVATE ${FFMPEG_INCLUDE_DIRS} ${ZSTD_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE ${FFMPEG_LIBRARIES} OpenSSL::Crypto ${ZSTD_LIBRARIES})
endfunction()

# Add custom executable for FFmpeg target builds
function(add_custom_executable_ffmpeg name src_file)
    add_executable(${name} ${src_file})
    target_include_directories(${name} PRIVATE ${FFMPEG_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE ${FFMPEG_LIBRARIES})
endfunction()

if (TBB_FOUND)
    message(STATUS "Found oneTBB: ${TBB_LIBRARIES}")
    include_directories(${TBB_INCLUDE_DIRS})
    link_directories(${TBB_LIBRARY_DIRS})

    add_custom_executable(${OWNER_BIN} ${OWNER_SRC})
    target_link_libraries(${OWNER_BIN} PRIVATE Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem Boost::date_time Boost::regex Threads::Threads TBB::tbb ${ARCHIVE_LIB} OpenSSL::SSL)
endif()

########################### ADD FETCHER PLUGINS HERE #########################################
set(WAVY_FETCHER_PLUGIN_OUTPUT_PATH ${CMAKE_BINARY_DIR}/plugins/fetcher)
set(WAVY_FETCHER_PLUGIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libwavy/tsfetcher/methods)
message(STATUS "${BLUE} *** COMPILING FETCHER PLUGINS HERE *** ...${RESET}")
message(NOTICE "\n==================[ WAVY FETCH PLUGIN BUILD ]==================")
add_subdirectory(${WAVY_FETCHER_PLUGIN_PATH}/aggr)
# Add more here ...
message(NOTICE "===============================================================\n")
########################### END FETCHER PLUGINS HERE #########################################

########################### ADD AUDIO BACKEND PLUGINS HERE #########################################
set(WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH ${CMAKE_BINARY_DIR}/plugins/audio)
set(WAVY_AUDIO_BACKEND_PLUGIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libwavy/audio/backends)
message(STATUS "${BLUE} *** COMPILING AUDIO BACKEND PLUGINS HERE *** ...${RESET}")
message(NOTICE "\n==================[ WAVY AUDIO BACKEND PLUGIN BUILD ]==================")
add_subdirectory(${WAVY_AUDIO_BACKEND_PLUGIN_PATH}/miniaudio)
# Add more here ...
message(NOTICE "=========================================================================\n")
########################### END AUDIO BACKEND PLUGINS HERE #########################################

add_custom_executable_ffmpeg(${CLIENT_BIN} ${CLIENT_SRC})
message(STATUS "<< Client compilation with std-C++20... >>")
target_compile_features(${CLIENT_BIN} PRIVATE cxx_std_20)
target_link_libraries(${CLIENT_BIN} PRIVATE Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem Boost::date_time Boost::regex Threads::Threads OpenSSL::SSL OpenSSL::Crypto ${ARCHIVE_LIB} ${ZSTD_LIBRARIES})

add_library(wavy-server SHARED libwavy/server/serverHelper.cpp)

# Specify include directories
target_include_directories(wavy-server PRIVATE ${ZSTD_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})

# Link required libraries (PUBLIC as we need these symbols to be read by server.cpp at runtime)
target_link_libraries(wavy-server PUBLIC Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem OpenSSL::SSL OpenSSL::Crypto ${ARCHIVE_LIB} ${ZSTD_LIBRARIES})

# Now, link the shared library to the server executable
add_executable(${SERVER_BIN} src/Server.cpp)
target_include_directories(${SERVER_BIN} PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_features(${SERVER_BIN} PRIVATE cxx_std_20)
target_link_libraries(${SERVER_BIN} PRIVATE  ${ARCHIVE_LIB} ${ZSTD_LIBRARIES} wavy-server)

add_custom_target(format COMMAND clang-format -i src/*.cpp include/*.h)
add_custom_target(tidy COMMAND clang-tidy -fix src/*.cpp include/*.h --)

add_custom_target(remove COMMAND rm -f *.ts *.m3u8)

# Individual build targets
add_custom_target(build_owner COMMAND ${CMAKE_COMMAND} --build . --target ${OWNER_BIN})
add_custom_target(build_decoder COMMAND ${CMAKE_COMMAND} --build . --target ${DECODER_BIN})
add_custom_target(build_server COMMAND ${CMAKE_COMMAND} --build . --target ${SERVER_BIN})
add_custom_target(build_client COMMAND ${CMAKE_COMMAND} --build . --target ${CLIENT_BIN})

# Examples
if (DEFINED BUILD_EXAMPLES AND BUILD_EXAMPLES)
  message(STATUS "${PURPLE}Compiling examples for Wavy...${RESET}")
  add_subdirectory(examples/hls)
  add_subdirectory(examples/transcoder)
  add_subdirectory(examples/flac)
  add_subdirectory(examples/registry)
  add_subdirectory(examples/network)
  add_subdirectory(examples/abr)
  add_subdirectory(examples/decoder)
  add_subdirectory(examples/m3u8parser)
  add_subdirectory(examples/dispatcher)
endif()

if (DEFINED BUILD_UI AND BUILD_UI)
  message(STATUS "${PURPLE}Building Wavy-UI with libquwrof...${RESET}")
  add_subdirectory(ui/)
else()
  message(STATUS "${BLUE}Skipping Wavy-UI build...${RESET}")
endif()

if(DEFINED BUILD_TARGET)
    set(TARGET_BUILT ${BUILD_TARGET})
else()
    set(TARGET_BUILT "All")
endif()

include(CheckIncludeFile)
include(CheckIncludeFileCXX)

macro(check_headers group prefix)
  foreach(hdr IN LISTS ${group})
    string(TOUPPER "${hdr}" upper)
    string(REPLACE "." "_" upper ${upper})
    string(REPLACE "/" "_" upper ${upper})
    set(var "WAVY__HAS_${upper}")
    if(${prefix} STREQUAL "C")
      check_include_file(${hdr} ${var})
    else()
      check_include_file_cxx(${hdr} ${var})
    endif()
  endforeach()
endmacro()

set(STANDARD_C_HEADERS
  assert.h ctype.h errno.h fcntl.h float.h inttypes.h limits.h math.h setjmp.h
  signal.h stdarg.h stddef.h stdio.h stdlib.h string.h time.h unistd.h stdbool.h
  fenv.h sys/types.h stdint.h
)

set(STANDARD_CXX_HEADERS
  # I/O
  iostream fstream sstream syncstream print

  # Containers
  vector array list deque forward_list span
  map unordered_map multimap unordered_multimap
  set unordered_set multiset unordered_multiset
  queue stack

  # Algorithms and numeric
  algorithm numeric iterator execution

  # Strings and text
  string string_view charconv format regex

  # Utilities
  memory functional utility type_traits typeindex
  any optional variant tuple bitset bit
  limits cstddef cstdint climits cfloat

  # Error handling
  exception stdexcept system_error

  # Multithreading
  thread mutex shared_mutex future condition_variable
  atomic stop_token

  # Time
  chrono

  # Random
  random

  # Coroutines
  coroutine

  # Modern C++ concepts and meta
  concepts version source_location

  # Filesystem
  filesystem

  # Math
  cmath numbers

  # New in C++23 (conditionally supported)
  expected stacktrace
)

set(SYSTEM_HEADERS
  windows.h sys/stat.h sys/mman.h dirent.h pthread.h dlfcn.h complex
)

check_headers(STANDARD_C_HEADERS "C")
check_headers(STANDARD_CXX_HEADERS "CXX")
check_headers(SYSTEM_HEADERS "C")

if (DEFINED AUTOGEN_HEADER AND AUTOGEN_HEADER)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/libwavy/config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/autogen/config.h"
  )
  message(NOTICE "${PURPLE}${BOLD}${UNDERLINE}--> AUTOGENERATED HEADER 'config.h' TO ${CMAKE_CURRENT_SOURCE_DIR}/autogen/.${RESET}\n")
else()
  message(STATUS "${BLUE}Skipping autogeneration of config header.${RESET}")
endif()

message(STATUS "\n━━━━━━━━━━━━━━━━━ ${PROJECT_NAME} project v${PROJECT_VERSION} ━━━━━━━━━━━━━━━━━\n")
message(STATUS "┌─ Build Configuration for ${PROJECT_NAME} ────────────────────────")
message(STATUS "│ Operating System        : ${CMAKE_SYSTEM_NAME}")
message(STATUS "│ Build Type              : ${CMAKE_BUILD_TYPE}")
message(STATUS "│ Build System            : ${CMAKE_MAKE_PROGRAM}")
message(STATUS "│ Target(s)               : ${TARGET_BUILT}")
message(STATUS "│ CMake Version           : ${CMAKE_VERSION}")
message(STATUS "│ Compiler                : ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "│ Compiler Flags          : ${CMAKE_CXX_FLAGS}")
message(STATUS "│ Libwavy Dest            : ${CMAKE_SOURCE_DIR}/libwavy/")
message(STATUS "│ Executable Names        : ${OWNER_BIN}, ${CLIENT_BIN}, ${SERVER_BIN}")
message(STATUS "│ libquwrof               : ${CMAKE_SOURCE_DIR}/libquwrof/")
message(STATUS "│ Build Wavy-UI           : ${BUILD_UI}")
message(STATUS "│ C++ Standard            : ${CMAKE_CXX_STANDARD}")
message(STATUS "│ FFmpeg Libraries        : ${FFMPEG_LIBRARIES}")
message(STATUS "│ Boost Libraries         : Boost::log, Boost::system, Boost::thread, etc.")
message(STATUS "│ OpenSSL                 : ${OPENSSL_LIBRARIES}")
message(STATUS "│ ZSTD                    : ${ZSTD_LIBRARIES}")
message(STATUS "│ oneTBB                  : ${TBB_LIBRARIES}")
message(STATUS "│ Archive Library         : ${ARCHIVE_LIB}")
message(STATUS "│ Linker Executable       : ${CMAKE_LINKER}")
message(STATUS "│ Linker Flags            : ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "└──────────────────────────────────────────────────────────\n")
