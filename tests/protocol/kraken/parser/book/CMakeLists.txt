# tests/protocol/kraken/parser/book/CMakeLists.txt


# Unit test executable
add_executable(test_subscribe_ack_parse
    test_subscribe_ack_parse.cpp
)

# Link the main Wirekrak SDK
target_link_libraries(test_subscribe_ack_parse PRIVATE wirekrak)

# The include path is already provided by linking wirekrak,
# but we can make it explicit:
target_include_directories(test_subscribe_ack_parse PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Compile definitions
target_compile_definitions(test_subscribe_ack_parse PRIVATE WK_UNIT_TEST)

# Register the test with CTest
add_test(NAME SubscribeAckParse COMMAND test_subscribe_ack_parse)