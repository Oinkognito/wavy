# cmake/PkgConfigDetect.cmake

# Destination for .pc files
set(PKGCONFIG_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" CACHE STRING "Directory to install pkg-config files")

# Template source directory
set(WAVY_PKGCONFIG_DIR "${CMAKE_SOURCE_DIR}/pkg-config")

# Output paths
set(WAVY_SERVER_PC "${CMAKE_BINARY_DIR}/wavy-server.pc")
set(WAVY_FFMPEG_PC "${CMAKE_BINARY_DIR}/wavy-ffmpeg.pc")
set(WAVY_UTILS_PC "${CMAKE_BINARY_DIR}/wavy-utils.pc")

# Configure .pc files
configure_file(${WAVY_PKGCONFIG_DIR}/wavy-server.pc.in ${WAVY_SERVER_PC} @ONLY)
configure_file(${WAVY_PKGCONFIG_DIR}/wavy-ffmpeg.pc.in ${WAVY_FFMPEG_PC} @ONLY)
configure_file(${WAVY_PKGCONFIG_DIR}/wavy-utils.pc.in ${WAVY_UTILS_PC} @ONLY)

# Install .pc files
install(FILES
  ${WAVY_SERVER_PC}
  ${WAVY_FFMPEG_PC}
  ${WAVY_UTILS_PC}
  DESTINATION ${PKGCONFIG_INSTALL_DIR}
)

# Install libraries
install(TARGETS wavy-server wavy-ffmpeg wavy-utils
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# Install includes
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)
