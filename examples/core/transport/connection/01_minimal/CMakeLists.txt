# examples/core/transport/connection/01_minimal/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# How does Wirekrak report message shape and fragmentation?
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_transport_connection_01_minimal_kraken_ticker kraken_ticker.cpp)
wirekrak_add_core_example(wkc_transport_connection_01_minimal_coinbase_ticker coinbase_ticker.cpp)
wirekrak_add_core_example(wkc_transport_connection_01_minimal_bitfinex_ticker bitfinex_ticker.cpp)
wirekrak_add_core_example(wkc_transport_connection_01_minimal_bybit_ticker bybit_ticker.cpp)
wirekrak_add_core_example(wkc_transport_connection_01_minimal_bitstamp_ticker bitstamp_ticker.cpp)
