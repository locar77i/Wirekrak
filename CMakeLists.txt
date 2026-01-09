cmake_minimum_required(VERSION 3.25)

# -------------------------------------------------------------
# Project Metadata
# -------------------------------------------------------------
project(Wirekrak VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# -------------------------------------------------------------
# Dependencies via vcpkg
# -------------------------------------------------------------
message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")

find_package(simdjson REQUIRED)
find_package(spdlog REQUIRED)
find_package(CLI11 REQUIRED)


# -------------------------------------------------------------
# Create Wirekrak Library
# -------------------------------------------------------------
add_library(wirekrak INTERFACE)

target_include_directories(wirekrak INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(wirekrak INTERFACE

    simdjson::simdjson
    spdlog::spdlog
)


if (WIN32)
    # -------------------------------------------------------------
    # Windows TLS (SChannel) - No OpenSSL required
    # -------------------------------------------------------------
    target_compile_definitions(wirekrak INTERFACE WIREKRAK_USE_SCHANNEL)
    target_link_libraries(wirekrak INTERFACE
        winhttp
        crypt32
        secur32
    )
endif()


# -----------------------
# Telemetry configuration
# -----------------------
option(WIREKRAK_ENABLE_TELEMETRY_L1 "Enable Wirekrak Telemetry Level 1" ON)
option(WIREKRAK_ENABLE_TELEMETRY_L2 "Enable Wirekrak Telemetry Level 2" OFF)
option(WIREKRAK_ENABLE_TELEMETRY_L3 "Enable Wirekrak Telemetry Level 3" OFF)

# Enforce hierarchy
if(WIREKRAK_ENABLE_TELEMETRY_L3)
    set(WIREKRAK_ENABLE_TELEMETRY_L2 ON)
    set(WIREKRAK_ENABLE_TELEMETRY_L1 ON)
endif()

if(WIREKRAK_ENABLE_TELEMETRY_L2)
    set(WIREKRAK_ENABLE_TELEMETRY_L1 ON)
endif()

# Apply only to Wirekrak targets
if(WIREKRAK_ENABLE_TELEMETRY_L1)
    message(STATUS "Building with Wirekrak Telemetry L1 enabled")
    target_compile_definitions(wirekrak INTERFACE WIREKRAK_ENABLE_TELEMETRY_L1)
endif()

if(WIREKRAK_ENABLE_TELEMETRY_L2)
    message(STATUS "Building with Wirekrak Telemetry L2 enabled")
    target_compile_definitions(wirekrak INTERFACE WIREKRAK_ENABLE_TELEMETRY_L2)
endif()

if(WIREKRAK_ENABLE_TELEMETRY_L3)
    message(STATUS "Building with Wirekrak Telemetry L3 enabled")
    target_compile_definitions(wirekrak INTERFACE WIREKRAK_ENABLE_TELEMETRY_L3)
endif()


# -------------------------------------------------------------
# Tests
# -------------------------------------------------------------
option(WIREKRAK_BUILD_TESTS "Build Wirekrak tests" OFF)

if (WIREKRAK_BUILD_TESTS)
    message(STATUS "Building Wirekrak tests")
    enable_testing()
    add_subdirectory(tests)
endif()


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
option(WIREKRAK_BUILD_EXAMPLES "Build Wirekrak examples" OFF)

if (WIREKRAK_BUILD_EXAMPLES)
    message(STATUS "Building Wirekrak examples")
    add_subdirectory(examples)
endif()


# -------------------------------------------------------------
# Experimental integrations
# -------------------------------------------------------------
# Experimental integrations are gated behind an explicit CMake 
# option and preset to avoid impacting normal Debug/Release builds.
# ------------------------------------------------------------------
option(WIREKRAK_BUILD_EXPERIMENTAL "Build experimental / external integration examples" OFF)

if (WIREKRAK_BUILD_EXPERIMENTAL)
    message(STATUS "Building WireKrak experimental integrations")
    add_subdirectory(examples/experimental)
endif()


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------
option(WIREKRAK_BUILD_BENCHMARKS "Build Wirekrak benchmarks" OFF)

if (WIREKRAK_BUILD_BENCHMARKS)
    message(STATUS "Building Wirekrak benchmarks")
    add_subdirectory(benchmarks)
endif()


# -------------------------------------------------------------
# Wirekrak Lite (compiled facade)
# -------------------------------------------------------------
option(WIREKRAK_BUILD_LITE "Build Wirekrak Lite client library" ON)

if (WIREKRAK_BUILD_LITE)
    message(STATUS "Building WireKrak Lite client library")
    add_subdirectory(src)
endif()
