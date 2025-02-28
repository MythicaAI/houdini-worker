
# Add a test executable for test_frames.cpp
set(TEST_SOURCES "../net_buffer.h")

# Include the utest library from third_party
include_directories(../../../third_party/)

# include the scene talk library directory
include_directories(..)

# Add the test executable
add_executable(test_frames ${TEST_SOURCES} test_frames.cpp)

# Link any additional dependencies if necessary
# Example: target_link_libraries(test_frames my_library)

# Set up the test using the executable
add_test(NAME test_frames COMMAND test_frames)