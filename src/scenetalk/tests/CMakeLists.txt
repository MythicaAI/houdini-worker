
# Add a test executable for test_frames.cpp
set(TEST_SOURCES
        ../buffer_pool.h
        ../buffer_pool.cpp
        ../net_buffer.h
        ../net_buffer.cpp
        ../frame.h
        ../frame.cpp
        ../file_ref.h
        ../file_ref.cpp
        ../encoder.h
        ../encoder.cpp
        ../decoder.h
        ../decoder.cpp)

file(GLOB QCBOR_SOURCES
        "../../../third_party/qcbor/src/*.c")
message("${QCBOR_SOURCES}")

# Include the utest library from third_party
include_directories(../../../third_party/)

# include the scene talk library directory
include_directories(..)

# Add the test executable
add_executable(test_buffer_pool ${TEST_SOURCES} ${QCBOR_SOURCES} test_buffer_pool.cpp)
add_executable(test_frames ${TEST_SOURCES} ${QCBOR_SOURCES} test_frames.cpp)
add_executable(test_file_ref ${TEST_SOURCES} test_file_ref.cpp)
add_executable(test_net_buffer ${TEST_SOURCES} ${QCBOR_SOURCES} test_net_buffer.cpp)
add_executable(test_encoder ${TEST_SOURCES} ${QCBOR_SOURCES} test_encoder.cpp)
add_executable(test_decoder ${TEST_SOURCES} ${QCBOR_SOURCES} test_decoder.cpp)

# Set up the test using the executable
add_test(NAME test_buffer_pool COMMAND test_buffer_pool)
add_test(NAME test_file_ref COMMAND test_file_ref)
add_test(NAME test_net_buffer COMMAND test_net_buffer)
add_test(NAME test_encoder COMMAND test_encoder)
add_test(NAME test_decoder COMMAND test_decoder)
enable_testing()