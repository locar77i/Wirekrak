#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>

#include "wirekrak/lite/domain/trade.hpp"
#include "wirekrak/lite/domain/book_level.hpp"
#include "wirekrak/lite/error.hpp"


namespace wirekrak::lite {

// -----------------------------------------------------------------------------
// Client configuration
// -----------------------------------------------------------------------------
struct client_config {
    /// WebSocket endpoint used by the default exchange adapter.
    /// The exact exchange is an implementation detail.
    std::string endpoint = "wss://ws.kraken.com/v2";

    /// Reserved for future use.
    /// No guarantees are currently made about enforcement.
    std::chrono::milliseconds heartbeat_timeout{30'000};

    /// Reserved for future use.
    /// No guarantees are currently made about enforcement.
    std::chrono::milliseconds message_timeout{30'000};
};


/*
===============================================================================
Wirekrak Lite Client — v1 Public API (STABLE)
===============================================================================

The Lite Client is the stable, user-facing façade for consuming market data.

Lite v1 guarantees:
  - Stable domain value layouts
  - Stable callback signatures
  - Exchange-agnostic public API
  - No protocol or Core internals exposed
  - No breaking changes without a major version bump

The underlying exchange implementation is an internal detail.
===============================================================================
*/
class Client {
public:
    using trade_handler = std::function<void(const domain::Trade&)>;
    using book_handler  = std::function<void(const domain::BookLevel&)>;
    using error_handler = std::function<void(const Error&)>;

    // Construct a client using an explicit endpoint.
    explicit Client(std::string endpoint);
    // Construct a client using a configuration object.
    explicit Client(client_config cfg = {});
    // Non-copyable
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    // Destructor
    ~Client();

    // lifecycle
    bool connect();
    void disconnect();
    void poll();

    // error handling
    void on_error(error_handler cb);

    // -----------------------------
    // Trade subscriptions
    // -----------------------------
    void subscribe_trades(std::vector<std::string> symbols, trade_handler cb, bool snapshot = true);

    void unsubscribe_trades(std::vector<std::string> symbols);

    // -----------------------------
    // Book subscriptions
    // -----------------------------
    void subscribe_book(std::vector<std::string> symbols, book_handler cb, bool snapshot = true);

    void unsubscribe_book(std::vector<std::string> symbols);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wirekrak::lite
