cmake_minimum_required(VERSION 3.10)
project(houdini_worker)

# Mongoose library
add_library(mongoose STATIC
    third_party/mongoose/mongoose.c
)

target_include_directories(mongoose PUBLIC
    third_party/mongoose
)

# Houdini library
set(CMAKE_PREFIX_PATH $ENV{HFS}/toolkit/cmake)

find_package(Houdini REQUIRED)

# Main executable
file(GLOB SOURCES "src/*.cpp")

set(executable_name houdini_worker)
add_executable(${executable_name} ${SOURCES})

target_link_libraries(${executable_name} 
    Houdini 
    HoudiniThirdParty
    mongoose
)

# Configure target properties
houdini_configure_target(${executable_name}) 