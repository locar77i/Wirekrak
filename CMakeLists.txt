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
    winhttp
    simdjson::simdjson
    spdlog::spdlog
)



# -------------------------------------------------------------
# Windows TLS (SChannel)
# No OpenSSL required
# -------------------------------------------------------------
target_compile_definitions(wirekrak INTERFACE WIREKRAK_USE_SCHANNEL)

target_link_libraries(wirekrak INTERFACE
    crypt32
    secur32
)

# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
add_subdirectory(examples)

# -------------------------------------------------------------
# Tests
# -------------------------------------------------------------
option(WIREKRAK_BUILD_TESTS "Build Wirekrak tests" OFF)

if (WIREKRAK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


# ------------------------------------------------------------------
# Experimental wirekrak builds
# ----------------------------
# Experimental integrations are gated behind an explicit CMake 
# option and preset to avoid impacting normal Debug/Release builds.
# ------------------------------------------------------------------
option(WIREKRAK_EXPERIMENTAL_BUILD "Build experimental / external integration examples" OFF)
