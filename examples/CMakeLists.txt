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

add_executable(wirekrak_trades trades.cpp)
target_link_libraries(wirekrak_trades
    PRIVATE
        wirekrak
        CLI11::CLI11
)

add_executable(wirekrak_book_updates book_updates.cpp)
target_link_libraries(wirekrak_book_updates
    PRIVATE
        wirekrak
        CLI11::CLI11
)

add_executable(wirekrak_minimal_for_trades minimal_for_trades.cpp)
target_link_libraries(wirekrak_minimal_for_trades PRIVATE wirekrak)

add_executable(wirekrak_minimal_for_book_updates minimal_for_book_updates.cpp)
target_link_libraries(wirekrak_minimal_for_book_updates PRIVATE wirekrak)
