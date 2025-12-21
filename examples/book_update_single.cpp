#include <iostream>
#include <thread>
#include <chrono>

#include "wirekrak/winhttp/client.hpp"
#include "wirekrak/protocol/kraken/book/subscribe.hpp"
#include "wirekrak/protocol/kraken/book/update.hpp"
#include "wirekrak/protocol/kraken/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/book/unsubscribe.hpp"

using namespace wirekrak;
using namespace wirekrak::protocol::kraken;
using Logger = lcr::log::Logger;
using Level  = lcr::log::Level;


int main() {
    Logger::instance().set_level(Level::Info);

    winhttp::WinClient client;
    if (!client.connect("wss://ws.kraken.com/v2")) {
        return -1;
    }

    // Subscribe to BTC/USD book updates
    client.subscribe(book::Subscribe{.symbols = {"BTC/USD"}},
                     [](const book::Update& msg) {
                        std::cout << " -> " << msg << std::endl;
                     }
    );
    
    // MAIN POLLING LOOP
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Unsubscribe from BTC/USD book updates
    client.unsubscribe(book::Unsubscribe{.symbols = {"BTC/USD"}});
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((client.book_subscriptions().has_pending() || client.book_subscriptions().has_active()) && std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[wirekrak] Heartbeats received so far: " << client.heartbeat_total() << std::endl;
    return 0;
}
