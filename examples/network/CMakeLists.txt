cmake_minimum_required(VERSION 3.22)
project(wavy_examples LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMake's way of finding Boost since Boost C++ 1.70
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find PkgConfig (needed if you later hook FFmpeg, but not used here)
find_package(PkgConfig REQUIRED)

# Example 1: simple GET request
add_executable(example_get_req main.cpp)
target_include_directories(example_get_req PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(example_get_req PRIVATE
    wavy-logger
    OpenSSL::SSL OpenSSL::Crypto
)
target_compile_definitions(example_get_req PRIVATE BOOST_LOG_DLL)

# Example 2: chunked streaming request
add_executable(example_chunked_req chunked.cpp)
target_include_directories(example_chunked_req PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(example_chunked_req PRIVATE
    wavy-logger
    OpenSSL::SSL OpenSSL::Crypto
)
target_compile_definitions(example_chunked_req PRIVATE BOOST_LOG_DLL)
