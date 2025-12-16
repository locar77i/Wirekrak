# tests/wal/recorder/CMakeLists.txt

# Unit test executable
add_executable(test_controller
    test_Controller.cpp
)

# Link the main Wirekrak SDK
target_link_libraries(test_controller PRIVATE wirekrak)

# The include path is already provided by linking wirekrak,
# but we can make it explicit:
target_include_directories(test_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Compile definitions
target_compile_definitions(test_controller PRIVATE WK_UNIT_TEST)

# Register the test with CTest
add_test(NAME RecorderControllerTest COMMAND test_controller)
