cmake_minimum_required(VERSION 3.22)
project(example_get_req LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES main.cpp)

# CMake's way of finding Boost since Boost C++ 1.70
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Find Boost
find_package(Boost REQUIRED COMPONENTS log log_setup system thread)
find_package(OpenSSL REQUIRED)

# Find FFmpeg libraries
find_package(PkgConfig REQUIRED)

# Executable
add_executable(example_get_req ${SOURCES})

# Include directories
target_include_directories(example_get_req PRIVATE ${Boost_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})

# Link Libraries
target_link_libraries(example_get_req PRIVATE 
    Boost::log Boost::log_setup Boost::system Boost::thread
    OpenSSL::SSL OpenSSL::Crypto
)

# Definitions (for BOOST_LOG_DLL if needed)
target_compile_definitions(example_get_req PRIVATE BOOST_LOG_DLL)
