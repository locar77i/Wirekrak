# integrations/matching_engine/CMakeLists.txt


# flashstrike_matching_gateway
add_executable(flashstrike_matching_gateway
    flashstrike_matching_gateway.cpp
)
target_link_libraries(flashstrike_matching_gateway
    PRIVATE
        wirekrak-lite
        CLI11::CLI11
)
target_include_directories(flashstrike_matching_gateway
    PRIVATE
        ${PROJECT_SOURCE_DIR}/examples
        ${PROJECT_SOURCE_DIR}/integrations/include
)

target_compile_features(flashstrike_matching_gateway PRIVATE cxx_std_20)
