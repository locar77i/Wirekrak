# tests/winhttp/CMakeLists.txt

# Unit test executable
add_executable(test_websocket_transport
    test_websocket_transport.cpp
)

# Link the main Wirekrak SDK
target_link_libraries(test_websocket_transport PRIVATE wirekrak)

# The include path is already provided by linking wirekrak,
# but we can make it explicit:
target_include_directories(test_websocket_transport PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Compile definitions
target_compile_definitions(test_websocket_transport PRIVATE WK_UNIT_TEST)

# Register the test with CTest
add_test(NAME WebSocketTransporTest COMMAND test_websocket_transport)
