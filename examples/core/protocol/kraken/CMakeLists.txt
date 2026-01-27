# examples/core/protocol/kraken/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Ping
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_protocol_kraken_ping_pong ping_pong.cpp)


# -------------------------------------------------------------
# Trade
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_protocol_kraken_minimal_trade_subscription minimal_trade_subscription.cpp)
wirekrak_add_core_example(wkc_protocol_kraken_trade_subscription trade_subscription.cpp CLI11::CLI11)


# -------------------------------------------------------------
# Book
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_protocol_kraken_minimal_book_subscription minimal_book_subscription.cpp)
wirekrak_add_core_example(wkc_protocol_kraken_book_subscription book_subscription.cpp CLI11::CLI11)
