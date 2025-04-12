set(PLUGIN_TYPE "Aggressive")

set(PLUGIN_NAME wavy_aggr_fetch_plugin)
set(PLUGIN_SRC src/entry.cpp)

message(STATUS "  >>> Building FETCH PLUGIN: ${PLUGIN_TYPE}")
message(STATUS "  >>> Target: ${PLUGIN_NAME}")
message(STATUS "  >>> Output Directory: ${CMAKE_BINARY_DIR}/plugins\n")

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SRC})
target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${ZSTD_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME} PRIVATE Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem OpenSSL::SSL OpenSSL::Crypto ${ZSTD_LIBRARIES})

set_target_properties(${PLUGIN_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${WAVY_PLUGIN_OUTPUT_PATH}
)
