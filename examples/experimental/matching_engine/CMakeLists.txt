# examples/experimental/matching_engine/CMakeLists.txt

# ----------------------
# Option to enable METRICS
# ----------------------
option(ENABLE_FS_METRICS "Enable FS runtime metrics collection" ON)
if(ENABLE_FS_METRICS)
    message(STATUS "Building with FS runtime metrics enabled")
    add_compile_definitions(ENABLE_FS_METRICS)
endif()
option(ENABLE_FS1_METRICS "Enable FS1 runtime metrics collection" ON)
if(ENABLE_FS1_METRICS)
    message(STATUS "Building with FS1 runtime metrics enabled")
    add_compile_definitions(ENABLE_FS1_METRICS)
endif()
option(ENABLE_FS2_METRICS "Enable FS2 runtime metrics collection" ON)
if(ENABLE_FS2_METRICS)
    message(STATUS "Building with FS2 runtime metrics enabled")
    add_compile_definitions(ENABLE_FS2_METRICS)
endif()
option(ENABLE_FS3_METRICS "Enable FS3 runtime metrics collection" ON)
if(ENABLE_FS3_METRICS)
    message(STATUS "Building with FS3 runtime metrics enabled")
    add_compile_definitions(ENABLE_FS3_METRICS)
endif()


if (WIREKRAK_BUILD_EXPERIMENTAL)

    message(STATUS "Building WireKrak experimental integrations")


    # flashstrike_matching_gateway
    add_executable(flashstrike_matching_gateway
        flashstrike_matching_gateway.cpp
    )
    target_link_libraries(flashstrike_matching_gateway
        PRIVATE
            wirekrak
            CLI11::CLI11
    )
    target_include_directories(flashstrike_matching_gateway
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_compile_features(flashstrike_matching_gateway PRIVATE cxx_std_20)



    # flashstrike_instrument_engine_gateway
    add_executable(flashstrike_instrument_engine_gateway
        flashstrike_instrument_engine_gateway.cpp
    )
    target_link_libraries(flashstrike_instrument_engine_gateway
        PRIVATE
            wirekrak
            CLI11::CLI11
    )
    target_include_directories(flashstrike_instrument_engine_gateway
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_compile_features(flashstrike_instrument_engine_gateway PRIVATE cxx_std_20)



endif()
