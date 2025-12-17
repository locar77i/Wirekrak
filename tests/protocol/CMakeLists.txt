# tests/protocol/CMakeLists.txt

# Unit test executable
add_executable(test_liveness_detection
    test_liveness_detection.cpp
)

# Link the main Wirekrak SDK
target_link_libraries(test_liveness_detection PRIVATE wirekrak)

# The include path is already provided by linking wirekrak,
# but we can make it explicit:
target_include_directories(test_liveness_detection PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Compile definitions
target_compile_definitions(test_liveness_detection PRIVATE WK_UNIT_TEST)

# Register the test with CTest
add_test(NAME Liveness COMMAND test_liveness_detection)


# ADD SUBDIRS
add_subdirectory(kraken)
