# examples/core/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Transport winhttp
# -------------------------------------------------------------
wirekrak_add_core_example(wk_core_winhttp_trade_update_subscription transport/winhttp/trade_update_subscription.cpp)
wirekrak_add_core_example(wk_core_winhttp_book_snapshot_subscription transport/winhttp/book_snapshot_subscription.cpp)


# -------------------------------------------------------------
# Ping
# -------------------------------------------------------------
wirekrak_add_core_example(wk_core_ping_pong ping_pong.cpp)


# -------------------------------------------------------------
# Trade
# -------------------------------------------------------------
wirekrak_add_core_example(wk_core_minimal_trade_subscription minimal_trade_subscription.cpp)
wirekrak_add_core_example(wk_core_trade_subscription trade_subscription.cpp CLI11::CLI11)


# -------------------------------------------------------------
# Book
# -------------------------------------------------------------
wirekrak_add_core_example(wk_core_minimal_book_subscription minimal_book_subscription.cpp)
wirekrak_add_core_example(wk_core_book_subscription book_subscription.cpp CLI11::CLI11)
