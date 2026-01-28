# examples/core/transport/connection/02_delivery/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Receiving data on the wire is not the same as delivering data to the user.
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_transport_connection_02_delivery_kraken_trade kraken_trade.cpp)
wirekrak_add_core_example(wkc_transport_connection_02_delivery_coinbase_trade coinbase_trade.cpp)
