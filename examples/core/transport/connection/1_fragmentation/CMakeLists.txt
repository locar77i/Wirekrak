# examples/core/transport/connection/1_fragmentation/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# How does Wirekrak report message shape and fragmentation?
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_transport_connection_1_fragmentation_kraken_book kraken_book.cpp)
wirekrak_add_core_example(wkc_transport_connection_1_fragmentation_bitfinex_book bitfinex_book.cpp)
