# src/lite/CMakeLists.txt

add_library(wirekrak-lite STATIC
    enums.cpp
    domain/trade.cpp
    domain/book_level.cpp
    kraken/client.cpp
)

target_include_directories(wirekrak-lite
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(wirekrak-lite PUBLIC cxx_std_20)

# -------------------------------------------------------------
# Core deps
# -------------------------------------------------------------
target_link_libraries(wirekrak-lite
    PUBLIC
        wirekrak            # Core (INTERFACE)
        simdjson::simdjson
        spdlog::spdlog
)

# -------------------------------------------------------------
# Backend selection
# -------------------------------------------------------------
target_link_libraries(wirekrak-lite
    PUBLIC
        #wirekrak_backend_winhttp
        wirekrak_backend_asio
)
