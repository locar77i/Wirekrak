# cmake/Wirekrak.Examples.cmake


# ==============================================================================
# Wirekrak example helpers
# ==============================================================================

function(wirekrak_add_core_example target source)
    add_executable(${target} ${source})

    target_include_directories(${target}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/examples/
    )

    target_link_libraries(${target}
        PRIVATE
            wirekrak
            ${ARGN}
    )
endfunction()


function(wirekrak_add_lite_example target source)
    add_executable(${target} ${source})

    target_include_directories(${target}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/examples/
    )

    target_link_libraries(${target}
        PRIVATE
            wirekrak-lite
            ${ARGN}
    )
endfunction()
