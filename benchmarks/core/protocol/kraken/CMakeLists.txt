# benchmarks/protocol/kraken/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakBenchmarks.cmake)


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------

# WinHTTP benchmark (Windows only)
if (WIN32)
    wirekrak_add_benchmark(wkc_protocol_kraken_winhttp_single_book_subscription single_book_subscription.cpp wirekrak_backend_winhttp)
    wirekrak_add_benchmark(wkc_protocol_kraken_winhttp_multi_subscription multi_subscription.cpp wirekrak_backend_winhttp)
endif()

# Asio benchmark (Linux / optional on Windows)
if (UNIX OR WIN32)
    wirekrak_add_benchmark(wkc_protocol_kraken_asio_beast_single_book_subscription single_book_subscription.cpp wirekrak_backend_asio)
    wirekrak_add_benchmark(wkc_protocol_kraken_asio_beast_multi_subscription multi_subscription.cpp wirekrak_backend_asio)
endif()
