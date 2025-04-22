set(AUD_BACKEND_TITLE "PulseAudio")

set(AUD_BACKEND_NAME wavy_audio_backend_${AUD_BACKEND_TITLE}_plugin)
set(AUD_BACKEND_SRC src/entry.cpp)

message(STATUS "  >>> Building AUDIO BACKEND PLUGIN: ${AUD_BACKEND_TITLE}")
message(STATUS "  >>> Target: ${AUD_BACKEND_NAME}")
message(STATUS "  >>> Output Directory: ${WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH}\n")

set(WAVY_AUDIO_PLUGIN_ENTRIES "${WAVY_AUDIO_PLUGIN_ENTRIES}${AUD_BACKEND_TITLE}|${AUD_BACKEND_NAME}\n" PARENT_SCOPE)

# Find PulseAudio using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSEAUDIO REQUIRED libpulse-simple)
pkg_check_modules(FFMPEG REQUIRED libavutil libavformat libavcodec libswresample)

add_library(${AUD_BACKEND_NAME} SHARED ${AUD_BACKEND_SRC})

target_include_directories(${AUD_BACKEND_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${ZSTD_INCLUDE_DIRS}
  ${PULSEAUDIO_INCLUDE_DIRS}
  ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(${AUD_BACKEND_NAME} PRIVATE
  ${ZSTD_LIBRARIES}
  ${PULSEAUDIO_LIBRARIES}
  ${FFMPEG_LIBRARIES}
)

set_target_properties(${AUD_BACKEND_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH}
)
