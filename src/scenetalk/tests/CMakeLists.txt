
# Add a test executable for test_frames.cpp
set(TEST_SOURCES
        ../buffer_pool.h
        ../buffer_pool.cpp
        ../frame_decoder.h
        ../frame_decoder.cpp
        ../frame.h
        ../frame.cpp
        ../file_ref.h
        ../file_ref.cpp
        ../encoder.h
        ../encoder.cpp
        ../reader.h
        ../reader.cpp
        ../decode_reader_items.cpp)

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
add_executable(test_file_ref ${TEST_SOURCES} ${QCBOR_SOURCES} test_file_ref.cpp)
add_executable(test_frame_decoder ${TEST_SOURCES} ${QCBOR_SOURCES} test_frame_decoder.cpp)
add_executable(test_encoder ${TEST_SOURCES} ${QCBOR_SOURCES} test_encoder.cpp)
add_executable(test_reader ${TEST_SOURCES} ${QCBOR_SOURCES} test_reader.cpp)

# Set up the test using the executable
add_test(NAME test_buffer_pool COMMAND test_buffer_pool)
add_test(NAME test_file_ref COMMAND test_file_ref)
add_test(NAME test_frame_decoder COMMAND test_frame_decoder)
add_test(NAME test_encoder COMMAND test_encoder)
add_test(NAME test_reader COMMAND test_reader)
enable_testing()