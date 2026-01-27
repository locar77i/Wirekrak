# examples/core/transport/connection/03_lifecycle/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Error & Close Lifecycle
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_transport_connection_03_lifecycle_kraken_trade kraken_trade.cpp)
wirekrak_add_core_example(wkc_transport_connection_03_lifecycle_binance_trade binance_trade.cpp)
