cmake_minimum_required(VERSION 3.25)

# External integrations are not part of the main library
# They are standalone executables


find_package(lz4 CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)


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


# -------------------------------------------------------------
# Matching Engine (integration)
# -------------------------------------------------------------
add_subdirectory(order_book)
add_subdirectory(matching_engine)
