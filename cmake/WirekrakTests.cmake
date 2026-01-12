# cmake/WirekrakTestHelpers.cmake

# ==============================================================================
# Wirekrak test helpers
# ==============================================================================

function(wirekrak_add_test target source)
    add_executable(${target} ${source})

    target_link_libraries(${target} PRIVATE wirekrak)

    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/tests
    )

    target_compile_definitions(${target} PRIVATE WK_UNIT_TEST)

    add_test(NAME ${target} COMMAND ${target})
endfunction()
