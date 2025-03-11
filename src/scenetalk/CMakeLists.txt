cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(houdini_worker)
set(executable_name scenetalk-ex)
include_directories("../../third_party")
file(GLOB QCBOR_SOURCES "../../third_party/qcbor/src/*.c")
add_executable(${executable_name}
        frame_decoder.h
        encoder.h
        encoder.cpp
        example.cpp
        file_ref.h
        file_ref.cpp
        buffer_pool.h
        buffer_pool.cpp
        frame.h
        reader.h
        reader.cpp
        decode_reader_items.cpp
        ${QCBOR_SOURCES})

add_subdirectory(tests)