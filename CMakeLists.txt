cmake_minimum_required(VERSION 3.10)
project(houdini_worker)

# Mongoose library
add_library(mongoose STATIC
    third_party/mongoose/mongoose.c
)

target_include_directories(mongoose PUBLIC
    third_party/mongoose
)

# Setup the path for houdini toolkit (cmake support)
if(APPLE)
    # Use specific path for macOS builds
    set(Houdini_DIR "/Applications/Houdini/Houdini20.5.410/Frameworks/Houdini.framework/Versions/20.5/Resources/toolkit/cmake")
else()
    # For non-macOS platforms, use the HFS environment variable
    set(Houdini_DIR "$ENV{HFS}/toolkit/cmake")
endif()

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

add_subdirectory("src/scenetalk")