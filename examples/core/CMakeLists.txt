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
wirekrak_add_core_example(wk_core_trade_requests_minimal trade_requests_minimal.cpp)
wirekrak_add_core_example(wk_core_trade_requests trade_requests.cpp CLI11::CLI11)


# -------------------------------------------------------------
# Book
# -------------------------------------------------------------
wirekrak_add_core_example(wk_core_book_requests_minimal book_requests_minimal.cpp)
wirekrak_add_core_example(wk_core_book_requests book_requests.cpp CLI11::CLI11)
