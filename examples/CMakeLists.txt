# examples/CMakeLists.txt


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
add_executable(winhttp_trade_update_subscription transport/winhttp/trade_update_subscription.cpp)
target_link_libraries(winhttp_trade_update_subscription PRIVATE wirekrak)

add_executable(winhttp_book_snapshot_subscription transport/winhttp/book_snapshot_subscription.cpp)
target_link_libraries(winhttp_book_snapshot_subscription PRIVATE wirekrak)

add_executable(wirekrak_ping_pong ping_pong.cpp)
target_link_libraries(wirekrak_ping_pong PRIVATE wirekrak)

add_executable(wirekrak_trade_requests trade_requests.cpp)
target_link_libraries(wirekrak_trade_requests
    PRIVATE
        wirekrak
        CLI11::CLI11
)

add_executable(wirekrak_book_requests book_requests.cpp)
target_link_libraries(wirekrak_book_requests
    PRIVATE
        wirekrak
        CLI11::CLI11
)

add_executable(wirekrak_minimal_for_trade_requests minimal_for_trade_requests.cpp)
target_link_libraries(wirekrak_minimal_for_trade_requests PRIVATE wirekrak)

add_executable(wirekrak_minimal_for_book_requests minimal_for_book_requests.cpp)
target_link_libraries(wirekrak_minimal_for_book_requests PRIVATE wirekrak)
