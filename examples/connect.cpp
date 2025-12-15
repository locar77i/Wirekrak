#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <wirekrak/websocket.hpp>

int main() {
    boost::asio::io_context ioc;
    wirekrak::WebSocketTLS ws(ioc);

    ws.set_message_callback([](const std::string& msg){
        std::cout << "Received: " << msg << "\n";
    });

    ws.connect("ws.kraken.com", "443", "/v2");

    std::thread t([&](){ ioc.run(); });

    std::this_thread::sleep_for(std::chrono::seconds(5));

    ws.send(R"({"method":"ping"})");

    t.join();
    return 0;
}
