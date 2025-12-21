# examples/CMakeLists.txt


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
add_executable(winhttp_connect transport/winhttp/connect.cpp)
target_link_libraries(winhttp_connect PRIVATE wirekrak)

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

add_executable(wirekrak_trade_multi trade_multi.cpp)
target_link_libraries(wirekrak_trade_multi PRIVATE wirekrak)
