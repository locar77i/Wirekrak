# examples/experimental/matching_engine/CMakeLists.txt


if (WIREKRAK_BUILD_EXPERIMENTAL)

    message(STATUS "Building WireKrak experimental integrations")

    add_executable(wirekrak_external_integration
        external_integration.cpp
    )

    target_link_libraries(wirekrak_external_integration
        PRIVATE
            wirekrak
    )

    target_compile_features(wirekrak_external_integration PRIVATE cxx_std_20)

endif()
