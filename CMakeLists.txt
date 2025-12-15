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

find_package(Boost REQUIRED COMPONENTS asio beast system date_time regex container)
find_package(simdjson REQUIRED)
find_package(spdlog REQUIRED)

# -------------------------------------------------------------
# Source Files
# -------------------------------------------------------------
set(WIREKRAK_SOURCES
    src/winhttp/websocket.cpp
)

# -------------------------------------------------------------
# Create Wirekrak Library
# -------------------------------------------------------------
add_library(wirekrak ${WIREKRAK_SOURCES})

if (WIN32)
    target_link_libraries(wirekrak PUBLIC winhttp)
endif()

target_include_directories(wirekrak
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -------------------------------------------------------------
# Link Dependencies
# -------------------------------------------------------------
target_link_libraries(wirekrak
    PUBLIC
        Boost::asio
        Boost::beast
        Boost::system
        Boost::date_time
        Boost::regex
        Boost::container
        simdjson::simdjson
        spdlog::spdlog
)

# -------------------------------------------------------------
# Windows TLS (SChannel)
# No OpenSSL required
# -------------------------------------------------------------
if (WIN32)
    target_compile_definitions(wirekrak PUBLIC WIREKRAK_USE_SCHANNEL)
    target_link_libraries(wirekrak PUBLIC crypt32 secur32)
endif()

# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
add_executable(wirekrak_connect examples/winhttp/connect.cpp)
target_link_libraries(wirekrak_connect PRIVATE wirekrak winhttp)

add_executable(wirekrak_trade_single examples/winhttp/trade_single.cpp)
target_link_libraries(wirekrak_trade_single PRIVATE wirekrak winhttp)

add_executable(wirekrak_trade_multi examples/winhttp/trade_multi.cpp)
target_link_libraries(wirekrak_trade_multi PRIVATE wirekrak winhttp)

#add_executable(wirekrak_trade examples/trade_stream.cpp)
#target_link_libraries(wirekrak_trade wirekrak)

#add_executable(wirekrak_orderbook examples/orderbook_stream.cpp)
#target_link_libraries(wirekrak_orderbook wirekrak)

# -------------------------------------------------------------
# Tests (optional, enable if Catch2 installed)
# -------------------------------------------------------------
option(WIREKRAK_BUILD_TESTS "Build Wirekrak tests" OFF)

if (WIREKRAK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
