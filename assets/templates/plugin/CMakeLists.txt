set(PLUGIN_TYPE "Test")

message(STATUS "  >>> Building FETCH PLUGIN: ${PLUGIN_TYPE}")
message(STATUS "  >>> Target: wavy_test_fetch_plugin")
message(STATUS "  >>> Output Directory: ${CMAKE_BINARY_DIR}/plugins")

set(WAVY_FETCHER_PLUGIN_ENTRIES "${WAVY_FETCHER_PLUGIN_ENTRIES}${FETCHER_PLUGIN_TYPE}|${FETCHER_PLUGIN_NAME}\n" PARENT_SCOPE)

set(PLUGIN_NAME wavy_test_fetch_plugin)
set(PLUGIN_SRC src/entry.cpp)

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SRC})
target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${ZSTD_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME} PRIVATE Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem OpenSSL::SSL OpenSSL::Crypto ${ZSTD_LIBRARIES})

set_target_properties(${PLUGIN_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${WAVY_PLUGIN_OUTPUT_PATH} # this is defined in root dir's CMake
)
