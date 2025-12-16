#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <cassert>

#include "wirekrak/client.hpp"
#include "wirekrak/core/transport/websocket.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/log/logger.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "[TEST FAILED] " << #expr \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)


namespace wirekrak {

struct MockWebSocket {
    using MessageCallback = std::function<void(const std::string&)>;
    using CloseCallback   = std::function<void()>;

    MessageCallback on_msg;
    CloseCallback   on_close;

    bool connected = false;
    int close_count = 0;

    bool connect(const std::string&, const std::string&, const std::string&) {
        connected = true;
        return true;
    }

    bool send(const std::string&) {
        return connected;
    }

    void close() {
        if (!connected) return;
        connected = false;
        close_count++;
        if (on_close) on_close();
    }

    void set_message_callback(MessageCallback cb) {
        on_msg = std::move(cb);
    }

    void set_close_callback(CloseCallback cb) {
        on_close = std::move(cb);
    }

    // Test helpers
    void emit_message(const std::string& msg) {
        if (on_msg) on_msg(msg);
    }
};


void test_liveness_detection() {
    using clock = std::chrono::steady_clock;

    std::cout << "[TEST] Liveness detection started" << std::endl;

    Client<MockWebSocket> client;

    TEST_CHECK(client.connect("wss://test"));
    std::cout << "[TEST] Connected" << std::endl;

    // Simulate receiving a normal message
    client.ws().emit_message(R"({"channel":"heartbeat"})");
    std::cout << "[TEST] Initial heartbeat processed" << std::endl;

    // Poll once → everything alive
    client.poll();
    TEST_CHECK(client.ws().close_count == 0);

    // ---- Case 1: messages stop, heartbeat still alive ----
    client.force_last_heartbeat(clock::now());               // heartbeat OK
    client.force_last_message(clock::now() - std::chrono::seconds(20)); // message stale
    client.poll();
    TEST_CHECK(client.ws().close_count == 0); // [X] no reconnect
    std::cout << "[TEST] Case 1 passed" << std::endl;

    // ---- Case 2: heartbeat stops, messages still flowing ----
    client.force_last_message(clock::now());                 // message OK
    client.force_last_heartbeat(clock::now() - std::chrono::seconds(20)); // heartbeat stale
    client.poll();
    TEST_CHECK(client.ws().close_count == 0); // [X] no reconnect
    std::cout << "[TEST] Case 2 passed" << std::endl;

    // ---- Case 3: both stale ----
    client.force_last_message(clock::now() - std::chrono::seconds(20));
    client.force_last_heartbeat(clock::now() - std::chrono::seconds(20));
    client.poll();
    TEST_CHECK(client.ws().close_count == 1); // [V] forced reconnect
    std::cout << "[TEST] Case 3 passed" << std::endl;

    std::cout << "[TEST] Liveness detection PASSED!" << std::endl;
}

} // namespace wirekrak


int main() {
    wirekrak::test_liveness_detection();
    return 0;
}
