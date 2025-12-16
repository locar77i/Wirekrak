#include <iostream>
#include <thread>
#include <chrono>

#include "wirekrak/winhttp/client.hpp"
#include "wirekrak/schema/trade/Subscribe.hpp"
#include "wirekrak/schema/trade/Unsubscribe.hpp"

using namespace wirekrak;


int main() {

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
    
    // MAIN POLLING LOOP
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Unsubscribe from BTC/USD trades
    client.unsubscribe(schema::trade::Unsubscribe{.symbols = {"BTC/USD"}});
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while ((client.trade_subscriptions().has_pending() || client.trade_subscriptions().has_active()) && std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[wirekrak] Heartbeats received so far: " << client.heartbeat_total() << std::endl;
    return 0;
}
