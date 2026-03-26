# cmake/Wirekrak.Benchmark.cmake


# ==============================================================================
# Wirekrak benchmark helpers
# ==============================================================================

function(wirekrak_add_benchmark target source backend)
    add_executable(${target} ${source})

    target_include_directories(${target}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/examples/
    )

    target_link_libraries(${target}
        PRIVATE
            ${backend}
            ${ARGN}
    )
endfunction()


