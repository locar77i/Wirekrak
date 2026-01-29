# examples/core/transport/connection/04_liveness/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Heartbeat-driven liveness
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_transport_connection_04_liveness_kraken_passive kraken_passive.cpp)
wirekrak_add_core_example(wkc_transport_connection_04_liveness_bitfinex_passive bitfinex_passive.cpp)
wirekrak_add_core_example(wkc_transport_connection_04_liveness_bybit_passive bybit_passive.cpp)
