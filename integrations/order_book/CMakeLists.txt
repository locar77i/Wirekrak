# integrations/matching_engine/CMakeLists.txt


# flashstrike_order_book_gateway
add_executable(flashstrike_order_book_gateway
    flashstrike_order_book_gateway.cpp
)
target_link_libraries(flashstrike_order_book_gateway
    PRIVATE
        wirekrak-lite
        CLI11::CLI11
)
target_include_directories(flashstrike_order_book_gateway
    PRIVATE
        ${PROJECT_SOURCE_DIR}/examples
        ${PROJECT_SOURCE_DIR}/integrations/include
)

target_compile_features(flashstrike_order_book_gateway PRIVATE cxx_std_20)
