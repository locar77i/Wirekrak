#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <locale>

#include "wirekrak/transport/winhttp/websocket.hpp"

using namespace wirekrak;


int main() {
    transport::winhttp::WebSocket ws;
    ws.set_message_callback([](const std::string& msg){
        std::cout << "Received: " << msg << std::endl;
    });

    if (!ws.connect("ws.kraken.com", "443", "/v2")) {
        std::cerr << "Connect failed\n";
        return 1;
    }

    // wait a bit for messages
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // send ping-like JSON if desired (Kraken uses subscriptions; this is just a placeholder)
    if (!ws.send(R"({"method":"ping"})")) {
        return 2;
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Subscribe to TRADE channel
    bool result = ws.send(R"({
        "method": "subscribe",
        "params": {
            "channel": "trade",
            "symbol": ["BTC/USD"]
        }
    })");
    if (!result) {
        return 3;
    }

    // Keep running forever
    std::cout << "Subscribed. Waiting for messages...\n";
    while (true) Sleep(1000);

    ws.close();
    return 0;
}
