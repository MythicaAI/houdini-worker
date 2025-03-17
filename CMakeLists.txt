cmake_minimum_required(VERSION 3.10)
project(houdini_worker)

# Remotery library
add_library(remotery STATIC
    third_party/remotery/Remotery.c
)

target_include_directories(remotery PUBLIC
    third_party/remotery
)

target_compile_definitions(remotery PUBLIC
    RMT_ENABLED=0
)

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
    set(JEMALLOC_LIB "/Applications/Houdini/Houdini20.5.410/Frameworks/Houdini.framework/Versions/20.5/Resources/dsolib/libjemalloc.so")
else()
    # For non-macOS platforms, use the HFS environment variable
    set(Houdini_DIR "$ENV{HFS}/toolkit/cmake")
    set(JEMALLOC_LIB "$ENV{HFS}/dsolib/libjemalloc.so")
endif()

find_package(Houdini REQUIRED)

# Main executable
file(GLOB SOURCES "src/*.cpp")

set(executable_name houdini_worker)
add_executable(${executable_name} ${SOURCES}
        third_party/utest/utest.h)

target_link_libraries(${executable_name}
    Houdini
    HoudiniThirdParty
    mongoose
    remotery
    ${JEMALLOC_LIB}
)

# Configure target properties
houdini_configure_target(${executable_name})

#add_subdirectory("src/scenetalk")