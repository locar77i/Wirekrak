# src/lite/CMakeLists.txt

add_library(wirekrak-lite STATIC
    dto/trade.cpp
    kraken/client.cpp
)

target_include_directories(wirekrak-lite
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(wirekrak-lite
    PUBLIC
        wirekrak            # Core (INTERFACE)
        simdjson::simdjson
        spdlog::spdlog
)

target_compile_features(wirekrak-lite PUBLIC cxx_std_20)

if (WIN32)
    target_link_libraries(wirekrak-lite
        PUBLIC
            winhttp
            crypt32
            secur32
    )
endif()
