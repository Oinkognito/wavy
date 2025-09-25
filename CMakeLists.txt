cmake_minimum_required(VERSION 3.30)
include(cmake/GenerateAudioPluginRegistry.cmake)
include(cmake/GenerateFetcherPluginRegistry.cmake)
include(cmake/CheckHeaders.cmake)

string(ASCII 27 ESC)
set(RESET "${ESC}[0m")
set(GREEN "${ESC}[32m")  # Success messages
set(YELLOW "${ESC}[33m") # Warning messages
set(BLUE   "${ESC}[34m")  # Information messages
set(PURPLE "${ESC}[35m")
set(PINK "${ESC}[95m")
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
option(USE_FMT "Use fmt library for logging" OFF)

set(WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH ${CMAKE_BINARY_DIR}/plugins/audio)
set(WAVY_AUDIO_PLUGIN_ENTRIES "")
set(WAVY_AUDIO_BACKEND_PLUGIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/audio-backends)
set(WAVY_AUDIO_BACKEND_CONFIG_HEADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/autogen/audioConfig.h")

set(WAVY_FETCHER_PLUGIN_OUTPUT_PATH ${CMAKE_BINARY_DIR}/plugins/fetcher)
set(WAVY_FETCHER_PLUGIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libwavy/tsfetcher/methods)
set(WAVY_FETCHER_CONFIG_HEADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/autogen/fetcherConfig.h")

# Check if CMake is using Ninja
if (DEFINED BUILD_NINJA AND BUILD_NINJA)
    message(STATUS "Using ${BLUE}${BOLD}${UNDERLINE}Ninja build system${RESET} for fast parallel builds")
    set(CMAKE_MAKE_PROGRAM ninja)
else()
  message(STATUS "Using ${CMAKE_GENERATOR}...")
endif()

if(USE_FMT)
    find_package(fmt REQUIRED)
    message(STATUS "Using ${BLUE}${BOLD}fmt library${RESET} for Wavy Logger")
endif()

# Option to enable mold linker
option(USE_MOLD "Use mold as the linker" OFF)

if(DEFINED USE_MOLD AND USE_MOLD)
    message(STATUS "Using ${BLUE}${BOLD}${UNDERLINE}mold linker${RESET}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold -Wl,--stats")
    set(CMAKE_LINKER mold)
else()
    message(STATUS "Using default linker")
endif()

# Enable diagnostics on compilation and linking
if (DEFINED COMPILE_REPORT AND COMPILE_REPORT)
 message(STATUS "${BLUE}${BOLD}${UNDERLINE}Requested for compile report.${RESET}")
 add_compile_options(-ftime-report)
endif()

# CheckHeaders.cmake
check_headers(STANDARD_C_HEADERS "C")
check_headers(STANDARD_CXX_HEADERS "CXX")
check_headers(SYSTEM_HEADERS "C")

set(OWNER_BIN wavy_owner)
set(SERVER_BIN wavy_server)
set(CLIENT_BIN wavy_client)

set(OWNER_SRC wavy/Owner.cc)
set(SERVER_SRC wavy/Server.cc)
set(CLIENT_SRC wavy/Client.cc)

set(LIBWAVY_LOGGER_SRC src/logger.cc)
set(LIBWAVY_SERVER_SRC src/server/Server.cc)

set(INDICATORS_HEADERS external/indicators/include/)

find_package(PkgConfig REQUIRED)

find_package(Boost REQUIRED COMPONENTS log log_setup system thread filesystem date_time regex)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_library(ARCHIVE_LIB archive)

########################### -- WAVY LOGGER (SHARED OBJ) -- #########################################
add_library(wavy-logger SHARED ${LIBWAVY_LOGGER_SRC})
target_include_directories(wavy-logger PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(wavy-logger PUBLIC Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem Boost::date_time Boost::regex)
########################### -- WAVY LOGGER (SHARED OBJ) -- #########################################

########################### -- WAVY SERVER (SHARED OBJ) -- #########################################
add_library(wavy-server SHARED ${LIBWAVY_SERVER_SRC})
target_include_directories(wavy-server PRIVATE ${ZSTD_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
# Link required libraries (PUBLIC as we need these symbols to be read by server.cpp at runtime)
target_link_libraries(wavy-server PUBLIC wavy-logger OpenSSL::SSL OpenSSL::Crypto ${ARCHIVE_LIB} ${ZSTD_LIBRARIES})
if(USE_FMT)
    target_link_libraries(wavy-logger PUBLIC fmt::fmt)
endif()
########################### -- WAVY SERVER (SHARED OBJ) -- #########################################

if (DEFINED NO_FFMPEG AND NO_FFMPEG)
  message(STATUS "${YELLOW}${BOLD}Skipping FFmpeg check...${RESET}")
else ()
  pkg_check_modules(FFMPEG REQUIRED libavutil libavformat libavcodec libswresample)
  file(GLOB FFMPEG_SOURCES "src/ffmpeg/*.cc")

  add_library(wavy-ffmpeg SHARED ${FFMPEG_SOURCES})
  target_include_directories(wavy-ffmpeg PRIVATE ${FFMPEG_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
  target_link_libraries(wavy-ffmpeg PUBLIC ${FFMPEG_LIBRARIES} wavy-logger)
endif()

if(ARCHIVE_LIB)
  message(STATUS "${BLUE}${BOLD}Found archive library: ${ARCHIVE_LIB}${RESET}")
else()
  message(WARNING "${YELLOW}${BOLD}Archive library not found${RESET}")
endif()

find_library(ZSTD_LIB libzstd)
pkg_check_modules(ZSTD REQUIRED libzstd)

if(ZSTD_FOUND)
  message(STATUS "${BLUE}${BOLD}pkg-config found Zstd: ${ZSTD_LIBRARIES}${RESET}")
  message(STATUS "${BLUE}${BOLD}${UNDERLINE}Zstd include directories: ${ZSTD_INCLUDE_DIRS}${RESET}")
else()
  message(WARNING "${YELLOW}${BOLD}pkg-config could not find Zstd${RESET}")
endif()

if (DEFINED NO_TBB AND NO_TBB)
  message(STATUS "${YELLOW}${BOLD}Skippping module Intel-oneTBB...${RESET}")
else()
  find_package(TBB REQUIRED tbb)
  pkg_check_modules(TBB REQUIRED tbb)
endif()

if (TBB_FOUND)
  message(STATUS "${BLUE}${BOLD}Found oneTBB: ${TBB_LIBRARIES}${RESET}")
    include_directories(${TBB_INCLUDE_DIRS})
    link_directories(${TBB_LIBRARY_DIRS})

    add_executable(${OWNER_BIN} ${OWNER_SRC})
    target_include_directories(${OWNER_BIN} PRIVATE ${FFMPEG_INCLUDE_DIRS} ${INDICATORS_HEADERS} ${CMAKE_SOURCE_DIR})
    target_link_libraries(${OWNER_BIN} PRIVATE wavy-ffmpeg wavy-logger ${ZSTD_LIBRARIES} Threads::Threads TBB::tbb ${ARCHIVE_LIB} OpenSSL::SSL)
endif()

if (DEFINED BUILD_FETCHER_PLUGINS AND BUILD_FETCHER_PLUGINS)
########################### ADD FETCHER PLUGINS HERE #########################################
message(NOTICE "\n${BLUE}${BOLD}${UNDERLINE} *** COMPILING FETCHER PLUGINS HERE *** ...${RESET}")
message(NOTICE "\n${GREEN}${BOLD}==================[ WAVY FETCH PLUGIN BUILD ]==================${RESET}")
add_subdirectory(${WAVY_FETCHER_PLUGIN_PATH}/aggr)
# Add more here ...
message(NOTICE "${GREEN}${BOLD}===============================================================${RESET}\n")
########################### END FETCHER PLUGINS HERE #########################################
else()
  message(STATUS "${YELLOW}${BOLD}${UNDERLINE}Skipping FETCHER PLUGIN build routine...${RESET}")
endif()

if (DEFINED BUILD_AUDIO_BACKEND_PLUGINS AND BUILD_AUDIO_BACKEND_PLUGINS)
########################### ADD AUDIO BACKEND PLUGINS HERE #########################################
message(NOTICE "\n${BLUE}${BOLD}${UNDERLINE} *** COMPILING AUDIO BACKEND PLUGINS HERE *** ...${RESET}")
message(NOTICE "\n${GREEN}${BOLD}==================[ WAVY AUDIO BACKEND PLUGIN BUILD ]===================${RESET}")
add_subdirectory(${WAVY_AUDIO_BACKEND_PLUGIN_PATH}/pulseaudio)
#add_subdirectory(${WAVY_AUDIO_BACKEND_PLUGIN_PATH}/alsa) # {TO TRY ALSA OUT JUST UNCOMMENT THIS LINE!}
# Add more here ...
message(NOTICE "${GREEN}${BOLD}=========================================================================${RESET}\n")
########################### END AUDIO BACKEND PLUGINS HERE #########################################
else()
  message(STATUS "${YELLOW}${BOLD}${UNDERLINE}Skipping AUDIO BACKEND PLUGIN build routine...${RESET}")
endif()

########################### -- MISC CPP FILES (WAVY-UTILS) -- #########################################
file(GLOB WAVY_UTILS_SOURCES "src/utils/*.cc")
add_library(wavy-utils SHARED ${WAVY_UTILS_SOURCES})
target_include_directories(wavy-utils PRIVATE ${FFMPEG_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
target_link_libraries(wavy-utils PUBLIC ${FFMPEG_LIBRARIES} wavy-logger)
########################### -- MISC CPP FILES (WAVY-UTILS) -- #########################################

########################### -- WAVY CLIENT -- #########################################
file(GLOB WAVY_COMPONENT_CLIENT_SOURCES "src/components/client/*.cc")
add_executable(${CLIENT_BIN} ${CLIENT_SRC} ${WAVY_COMPONENT_CLIENT_SOURCES})
target_include_directories(${CLIENT_BIN} PRIVATE ${FFMPEG_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
target_compile_features(${CLIENT_BIN} PRIVATE cxx_std_20)
target_link_libraries(${CLIENT_BIN} PRIVATE wavy-ffmpeg wavy-logger Threads::Threads OpenSSL::SSL OpenSSL::Crypto ${ARCHIVE_LIB} ${ZSTD_LIBRARIES})
########################### -- WAVY CLIENT -- #########################################

########################### -- WAVY SERVER -- #########################################
add_executable(${SERVER_BIN} ${SERVER_SRC})
target_include_directories(${SERVER_BIN} PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_features(${SERVER_BIN} PRIVATE cxx_std_20)
target_link_libraries(${SERVER_BIN} PRIVATE  ${ARCHIVE_LIB} ${ZSTD_LIBRARIES} wavy-server)
########################### -- WAVY SERVER -- #########################################

# Individual build targets
add_custom_target(build_owner COMMAND ${CMAKE_COMMAND} --build . --target ${OWNER_BIN})
add_custom_target(build_server COMMAND ${CMAKE_COMMAND} --build . --target ${SERVER_BIN})
add_custom_target(build_client COMMAND ${CMAKE_COMMAND} --build . --target ${CLIENT_BIN})

# Generate pkg-config files for shared obj libs
include(cmake/PkgConfigDetect.cmake)

# Examples
if (DEFINED BUILD_EXAMPLES AND BUILD_EXAMPLES)
  message(STATUS "${PURPLE}${BOLD}${UNDERLINE}Compiling examples for Wavy...${RESET}")
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
  message(STATUS "${PURPLE}${BOLD}${UNDERLINE}Building Wavy-UI with libquwrof...${RESET}")
  add_subdirectory(ui/)
else()
  message(STATUS "${BLUE}${BOLD}Skipping Wavy-UI build...${RESET}")
endif()

if(DEFINED BUILD_TARGET)
    set(TARGET_BUILT ${BUILD_TARGET})
else()
    set(TARGET_BUILT "All")
endif()

if (DEFINED AUTOGEN_HEADER AND AUTOGEN_HEADER)
  message(NOTICE "\n${GREEN}${BOLD}==================[ WAVY AUTOGEN HEADER JOB ]============================${RESET}")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/libwavy/config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/autogen/config.h"
  )
  message(NOTICE "${PURPLE}${BOLD}${UNDERLINE}--> AUTOGENERATED HEADER 'config.h' TO ${CMAKE_CURRENT_SOURCE_DIR}/autogen/.${RESET}")
  # GenerateAudioPluginRegistry.cmake contains this logic
  generate_audio_plugin_registry(
    ${WAVY_AUDIO_BACKEND_CONFIG_HEADER_PATH}
    "${WAVY_AUDIO_PLUGIN_ENTRIES}"
  )
  message(NOTICE "${PURPLE}${BOLD}${UNDERLINE}--> AUTOGENERATED HEADER 'audioConfig.h' TO ${CMAKE_CURRENT_SOURCE_DIR}/autogen/.${RESET}")
  generate_fetcher_plugin_registry(
    ${WAVY_FETCHER_CONFIG_HEADER_PATH}
    "${WAVY_FETCHER_PLUGIN_ENTRIES}"
  )
  message(NOTICE "${PURPLE}${BOLD}${UNDERLINE}--> AUTOGENERATED HEADER 'fetcherConfig.h' TO ${CMAKE_CURRENT_SOURCE_DIR}/autogen/.${RESET}")
  message(NOTICE "${GREEN}${BOLD}=========================================================================${RESET}")
else()
  message(STATUS "${BLUE}${BOLD}Skipping autogeneration of config header.${RESET}")
endif()

message(NOTICE "\n━━━━━━━━━━━━━━━━━━━━━━━  ${PINK}${BOLD}${PROJECT_NAME} project v${PROJECT_VERSION}${RESET}  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")
message(STATUS "┌─ Build Configuration for ${PROJECT_NAME} ────────────────────────")
message(STATUS "│ Operating System        : ${CMAKE_SYSTEM_NAME}")
message(STATUS "│ Build Type              : ${CMAKE_BUILD_TYPE}")
message(STATUS "│ Build System            : ${CMAKE_MAKE_PROGRAM}")
message(STATUS "│ Target(s)               : ${TARGET_BUILT}")
message(STATUS "│ CMake Version           : ${CMAKE_VERSION}")
message(STATUS "│ Compiler                : ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "│ Compiler Flags          : ${CMAKE_CXX_FLAGS}")
message(STATUS "│ Libwavy Dest            : ${CMAKE_SOURCE_DIR}/libwavy/")
message(STATUS "│ Autogen Dest            : ${CMAKE_SOURCE_DIR}/autogen/")
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
