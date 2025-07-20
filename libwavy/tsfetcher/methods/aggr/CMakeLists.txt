set(FETCHER_PLUGIN_TYPE "Aggressive")

set(FETCHER_PLUGIN_NAME wavy_aggr_fetch_plugin)
set(FETCHER_PLUGIN_SRC src/entry.cpp)

message(STATUS "  >>> Building FETCH PLUGIN: ${FETCHER_PLUGIN_TYPE}")
message(STATUS "  >>> Target: ${FETCHER_PLUGIN_NAME}")
message(STATUS "  >>> Output Directory: ${WAVY_FETCHER_PLUGIN_OUTPUT_PATH}\n")

set(WAVY_FETCHER_PLUGIN_ENTRIES "${WAVY_FETCHER_PLUGIN_ENTRIES}${FETCHER_PLUGIN_TYPE}|${FETCHER_PLUGIN_NAME}\n" PARENT_SCOPE)

add_library(${FETCHER_PLUGIN_NAME} SHARED ${FETCHER_PLUGIN_SRC})
target_include_directories(${FETCHER_PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${ZSTD_INCLUDE_DIRS})
target_link_libraries(${FETCHER_PLUGIN_NAME} PRIVATE wavy-utils Boost::log Boost::log_setup Boost::system Boost::thread Boost::filesystem OpenSSL::SSL OpenSSL::Crypto ${ZSTD_LIBRARIES})

set_target_properties(${FETCHER_PLUGIN_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${WAVY_FETCHER_PLUGIN_OUTPUT_PATH}
)
