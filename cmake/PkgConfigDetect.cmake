# cmake/PkgConfigDetect.cmake

# Logging helper
function(log_pkgconfig msg)
  message(STATUS "[PKG-CONF] ${msg}")
endfunction()

log_pkgconfig("Initializing pkg-config file generation...")

# Destination for .pc files
set(PKGCONFIG_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" CACHE STRING "Directory to install pkg-config files")
log_pkgconfig("Pkg-config files will be installed to: ${PKGCONFIG_INSTALL_DIR}")

# Template source directory
set(WAVY_PKGCONFIG_DIR "${CMAKE_SOURCE_DIR}/pkg-config")
log_pkgconfig("Using template .pc.in files from: ${WAVY_PKGCONFIG_DIR}")

# Output paths
set(WAVY_SERVER_PC "${CMAKE_BINARY_DIR}/wavy-server.pc")
set(WAVY_UTILS_PC  "${CMAKE_BINARY_DIR}/wavy-utils.pc")

# Configure mandatory .pc files
log_pkgconfig("Generating wavy-server.pc...")
configure_file(${WAVY_PKGCONFIG_DIR}/wavy-server.pc.in ${WAVY_SERVER_PC} @ONLY)

log_pkgconfig("Generating wavy-utils.pc...")
configure_file(${WAVY_PKGCONFIG_DIR}/wavy-utils.pc.in ${WAVY_UTILS_PC} @ONLY)

# Track install targets
set(WAVY_PC_FILES ${WAVY_SERVER_PC} ${WAVY_UTILS_PC})
set(WAVY_PC_TARGETS wavy-server wavy-utils)

# Conditional FFmpeg support
if (NOT DEFINED NO_FFMPEG OR NOT NO_FFMPEG)
  set(WAVY_FFMPEG_PC "${CMAKE_BINARY_DIR}/wavy-ffmpeg.pc")
  log_pkgconfig("FFmpeg support enabled — generating wavy-ffmpeg.pc...")
  configure_file(${WAVY_PKGCONFIG_DIR}/wavy-ffmpeg.pc.in ${WAVY_FFMPEG_PC} @ONLY)
  list(APPEND WAVY_PC_FILES ${WAVY_FFMPEG_PC})
  list(APPEND WAVY_PC_TARGETS wavy-ffmpeg)
else()
  log_pkgconfig("FFmpeg support disabled — skipping wavy-ffmpeg.pc.")
endif()

# Install .pc files
log_pkgconfig("Installing .pc files: ${WAVY_PC_FILES}")
install(FILES ${WAVY_PC_FILES}
        DESTINATION ${PKGCONFIG_INSTALL_DIR})

# Install libraries
log_pkgconfig("Installing libraries: ${WAVY_PC_TARGETS}")
install(TARGETS ${WAVY_PC_TARGETS}
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# Install headers
log_pkgconfig("Installing headers from ${CMAKE_SOURCE_DIR}/include/")
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)

log_pkgconfig("Pkg-config setup complete.\n")
