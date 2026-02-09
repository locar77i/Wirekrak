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

    // -----------------------------------------------------------------------------
    // Client quiescence indicator
    // -----------------------------------------------------------------------------
    //
    // Returns true if the Lite client is **idle**.
    //
    // Client-idle means that, at the current instant:
    //   • The underlying Core Session is protocol-idle
    //   • No registered subscribe or unsubscribe behaviors remain
    //   • No user-visible callbacks are waiting to be dispatched
    //
    // In other words:
    //   If poll() is never called again, no further user callbacks
    //   will be invoked and no protocol obligations remain outstanding.
    //
    // IMPORTANT SEMANTICS:
    //
    // • This is a *best-effort, instantaneous observation*.
    //   New data may arrive after this call returns true if the
    //   connection remains open.
    //
    // • This does NOT imply that there are no active subscriptions.
    //   Active subscriptions may continue to produce data in the future.
    //
    // • This does NOT close the connection or suppress future events.
    //
    // • This method is intended for **graceful shutdown and drain loops**,
    //   not for steady-state flow control.
    //
    // Layering guarantee:
    //
    // • Lite::Client::is_idle() composes on top of Core semantics.
    // • It does NOT introduce new protocol behavior.
    // • It does NOT expose Core internals.
    //
    // Threading & usage:
    //   • Not thread-safe
    //   • Must be called from the same thread as poll()
    //   • Typically used after unsubscribe() or before shutdown
    //
    // -----------------------------------------------------------------------------
    // Example usage:
    //
    //   // Drain until no more callbacks can fire
    //   while (!client.is_idle()) {
    //       client.poll();
    //   }
    //
    // -----------------------------------------------------------------------------
    bool is_idle() const;

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
