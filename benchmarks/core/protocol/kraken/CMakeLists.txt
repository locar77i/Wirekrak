# benchmarks/protocol/kraken/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakBenchmarks.cmake)


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------

# WinHTTP benchmark (Windows only)
if (WIN32)
    wirekrak_add_benchmark(wkc_protocol_kraken_winhttp_book_latency_btc_usd book_latency_btc_usd.cpp wirekrak_backend_winhttp)
    wirekrak_add_benchmark(wkc_protocol_kraken_winhttp_book_latency_usual_usd book_latency_usual_usd.cpp wirekrak_backend_winhttp)
    wirekrak_add_benchmark(wkc_protocol_kraken_winhttp_full_exchange_ingestion full_exchange_ingestion.cpp wirekrak_backend_winhttp)
endif()

# Asio benchmark (Linux / optional on Windows)
if (UNIX OR WIN32)
    wirekrak_add_benchmark(wkc_protocol_kraken_asio_beast_book_latency_btc_usd book_latency_btc_usd.cpp wirekrak_backend_asio)
    wirekrak_add_benchmark(wkc_protocol_kraken_asio_beast_book_latency_usual_usd book_latency_usual_usd.cpp wirekrak_backend_asio)
    wirekrak_add_benchmark(wkc_protocol_kraken_asio_beast_full_exchange_ingestion full_exchange_ingestion.cpp wirekrak_backend_asio)
endif()
