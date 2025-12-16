#include <iostream>
#include <thread>
#include <chrono>

#include "wirekrak/winhttp/client.hpp"
#include "wirekrak/schema/trade/Subscribe.hpp"
#include "wirekrak/schema/trade/Unsubscribe.hpp"


using namespace wirekrak;

#ifdef _WIN32
#include <windows.h>
bool enable_ansi_colors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;

    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return false;

    DWORD newMode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, newMode))
        return false;

    return true;
}
#endif


int main() {
#ifdef _WIN32
    lcr::log::Logger::instance().enable_color(enable_ansi_colors());
#else
    lcr::log::Logger::instance().enable_color(true);
#endif

    winhttp::WinClient client;
    if (!client.connect("wss://ws.kraken.com/v2")) {
        return -1;
    }

    // Subscribe to BTC/USD trades
    client.subscribe(schema::trade::Subscribe{.symbols = {"BTC/USD"}},
                     [](const schema::trade::Response& msg) {
                        std::cout << " -> [BTC/USD] TRADE: id=" << msg.trade_id << " price=" << msg.price << " qty=" << msg.qty << " side=" << to_string(msg.side) << std::endl;
                     }
    );

    // Subscribe to BTC/EUR trades
    client.subscribe(schema::trade::Subscribe{.symbols = {"BTC/EUR"}},
                     [](const schema::trade::Response& msg) {
                        std::cout << " -> [BTC/EUR] TRADE: id=" << msg.trade_id << " price=" << msg.price << " qty=" << msg.qty << " side=" << to_string(msg.side) << std::endl;
                     }
    );
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Unsubscribe from BTC/USD trades
    client.unsubscribe(schema::trade::Unsubscribe{.symbols = {"BTC/USD"}});
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while ((client.trade_subscriptions().has_pending() || client.trade_subscriptions().has_active()) && std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Unsubscribe from EUR/USD trades
    client.unsubscribe(schema::trade::Unsubscribe{.symbols = {"BTC/EUR"}});
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((client.trade_subscriptions().has_pending() || client.trade_subscriptions().has_active()) && std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Subscribe to trades on multiple symbols
    client.subscribe(schema::trade::Subscribe{.symbols = {"BTC/USD", "ETH/USD", "SOL/USD", "XRP/USD", "LTC/USD", "ADA/USD", "DOGE/USD", "DOT/USD", "LINK/USD", "ATOM/USD"}},
                     [](const schema::trade::Response& msg) {
                        std::cout << " -> TRADE: id=" << msg.trade_id << " price=" << msg.price << " qty=" << msg.qty << " side=" << to_string(msg.side) << std::endl;
                     }
    );
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Unsubscribe from five symbols at a time
    client.unsubscribe(schema::trade::Unsubscribe{.symbols = {"ADA/USD", "DOGE/USD", "DOT/USD", "LINK/USD", "ATOM/USD"}});
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while ((client.trade_subscriptions().has_pending() || client.trade_subscriptions().has_active()) && std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Unsubscribe from the remaining symbols
    client.unsubscribe(schema::trade::Unsubscribe{.symbols = {"BTC/USD", "ETH/USD", "SOL/USD", "XRP/USD", "LTC/USD"}});
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((client.trade_subscriptions().has_pending() || client.trade_subscriptions().has_active()) && std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[wirekrak] Heartbeats received so far: " << client.heartbeat_total() << std::endl;
    return 0;
}
