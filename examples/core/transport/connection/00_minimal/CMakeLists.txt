# examples/core/transport/connection/00_minimal/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Minimal connection examples
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_transport_connection_00_minimal_kraken kraken.cpp)
wirekrak_add_core_example(wkc_transport_connection_00_minimal_binance binance.cpp)
wirekrak_add_core_example(wkc_transport_connection_00_minimal_bitfinex bitfinex.cpp)
wirekrak_add_core_example(wkc_transport_connection_00_minimal_bitstamp bitstamp.cpp)
wirekrak_add_core_example(wkc_transport_connection_00_minimal_bybit bybit.cpp)
wirekrak_add_core_example(wkc_transport_connection_00_minimal_coinbase coinbase.cpp)
