#include <iostream>
#include <thread>
#include <chrono>

#include "wirekrak/winhttp/client.hpp"
#include "wirekrak/protocol/kraken/trade/subscribe.hpp"
#include "wirekrak/protocol/kraken/trade/unsubscribe.hpp"

using namespace wirekrak;
using namespace lcr::log;


int main() {
    Logger::instance().set_level(Level::Info);

    winhttp::WinClient client;
    if (!client.connect("wss://ws.kraken.com/v2")) {
        return -1;
    }
    std::cout << "[wirekrak] Connected to wss://ws.kraken.com/v2" << std::endl;
    // Subscribe to BTC/USD trades
    client.subscribe(protocol::kraken::trade::Subscribe{.symbols = {"BTC/USD"}, .snapshot = false},
                     [](const protocol::kraken::trade::Trade& msg) {
                        std::cout << " -> [" << msg.symbol << "] TRADE: id=" << msg.trade_id << " price=" << msg.price << " qty=" << msg.qty << " side=" << to_string(msg.side) << std::endl;
                     }
    );
    std::cout << "[wirekrak] Subscribed to BTC/USD trades" << std::endl;
    // MAIN POLLING LOOP
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Unsubscribe from BTC/USD trades
    client.unsubscribe(protocol::kraken::trade::Unsubscribe{.symbols = {"BTC/USD"}});
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((client.trade_subscriptions().has_pending() || client.trade_subscriptions().has_active()) && std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[wirekrak] Heartbeats received so far: " << client.heartbeat_total() << std::endl;
    return 0;
}
