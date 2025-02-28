cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(houdini_worker)
file(GLOB SOURCES "*.cpp")
set(executable_name scenetalk-ex)
include_directories("../../third_party")
add_executable(${executable_name} ${SOURCES}
        net_buffer.h
        encoder.h
        encoder.cpp
        example.cpp
        file_ref.h
        file_ref.cpp)

add_subdirectory(tests)