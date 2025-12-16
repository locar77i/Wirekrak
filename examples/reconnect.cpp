#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

#include "wirekrak/winhttp/client.hpp"
#include "wirekrak/schema/trade/Subscribe.hpp"

using namespace wirekrak;

#ifdef _WIN32
#include <windows.h>
bool enable_ansi_colors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return false;
    return SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#endif

int main() {
#ifdef _WIN32
    lcr::log::Logger::instance().enable_color(enable_ansi_colors());
#else
    lcr::log::Logger::instance().enable_color(true);
#endif

    std::atomic<int> trades{0};
    std::atomic<int> reconnects{0};

    winhttp::WinClient client;
/*
    // Optional: if you expose hooks
    client.on_reconnect([&] {
        reconnects++;
        std::cout << "\n[wirekrak] RECONNECTED" << std::endl;
    });
*/

    if (!client.connect("wss://ws.kraken.com/v2")) {
        std::cerr << "Failed to connect" << std::endl;
        return -1;
    }


    client.subscribe(
        schema::trade::Subscribe{.symbols = {"BTC/USD"}},
        [&](const schema::trade::Response& msg) {
            trades++;
            std::cout
                << " -> TRADE "
                << trades.load()
                << " id=" << msg.trade_id
                << " price=" << msg.price
                << " qty=" << msg.qty
                << " side=" << to_string(msg.side)
                << std::endl;
        }
    );

    std::cout << "[wirekrak] Connected. Waiting for trades..." << std::endl;

    auto test_duration = std::chrono::seconds(30);
    auto reconnect_delay = std::chrono::seconds(10);
    auto unsubscribe_delay = test_duration - std::chrono::seconds(2);
    bool forced_disconnect = false;
    bool unsubscribed = false;
    auto start = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::steady_clock::now() - start;
    while (elapsed_time < test_duration) {
        client.poll();
        // Force a disconnect after 10 seconds
        if (!forced_disconnect && elapsed_time > reconnect_delay) {
            std::cout << "\n[wirekrak] FORCING SOCKET CLOSE" << std::endl;
            client.reconnect();   // <— important
            forced_disconnect = true;
        }
        // Unsubscribe after test_duration - 2 seconds
        if (forced_disconnect && !unsubscribed && elapsed_time > unsubscribe_delay) {
            std::cout << "\n[wirekrak] UNSUBSCRIBING FROM TRADE CHANNEL" << std::endl;
            client.unsubscribe(schema::trade::Unsubscribe{.symbols = {"BTC/USD"}});
            unsubscribed = true;
        }
        // Sleep a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        elapsed_time = std::chrono::steady_clock::now() - start;
    }

    std::cout << "\n========== TEST SUMMARY ==========" << std::endl;
    std::cout << "Trades received   : " << trades.load() << "" << std::endl;
    //std::cout << "Reconnects        : " << reconnects.load() << "" << std::endl;
    std::cout << "Heartbeats total  : " << client.heartbeat_total() << "" << std::endl;

    if (forced_disconnect && trades > 0) {
        std::cout << "[wirekrak] Reconnection test PASSED" << std::endl;
        return 0;
    }

    std::cout << "[wirekrak] Reconnection test FAILED" << std::endl;
    return 1;
}
