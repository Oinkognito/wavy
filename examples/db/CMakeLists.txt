cmake_minimum_required(VERSION 3.22)
project(example_db LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES main.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LMDB REQUIRED lmdb)

add_executable(example_db ${SOURCES})

target_include_directories(example_db
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${LMDB_INCLUDE_DIRS}     # libuv headers
)

target_link_libraries(example_db PRIVATE ${LMDB_LIBRARIES})
