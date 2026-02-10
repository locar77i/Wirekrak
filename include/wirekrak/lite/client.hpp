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

    // -------------------------------------------------------------------------
    // About the convenience execution loops
    // -------------------------------------------------------------------------
    //
    // IMPORTANT DISTINCTION — TERMINATION AUTHORITY:
    //
    //   - run_until_idle()  → **library-owned termination**
    //   - run_while()       → **user-owned termination**
    //   - run_until()       → **user-owned termination (negative condition)**
    //
    // Semantics:
    //
    //   run_until_idle()
    //     - Exits when the client reaches protocol quiescence
    //     - Observes only internal state (no user intent)
    //     - Intended for drain loops and graceful teardown
    //
    //   run_while(cond)
    //     - Continues while the user condition is true
    //     - The library never infers stop intent
    //     - Intended for steady-state execution
    //
    //   run_until(stop)
    //     - Continues until the user condition becomes true
    //     - No idle checks, no draining, no implicit cleanup
    //     - Intended for signal-, time-, or event-driven shutdown
    //
    // None of these methods introduce:
    //   - background threads
    //   - hidden scheduling
    //   - protocol side effects
    //   - alternative execution models
    //
    // All execution remains explicitly poll-driven.
    //
    // Intended use:
    //
    //   - Graceful shutdown and drain loops
    //   - Deterministic teardown after unsubscribe()
    //   - CLI tools and short-lived programs
    //   - Tests and examples that need protocol completion
    //
    // NOTE:
    // If tick == 0, these loops will busy-wait by continuously
    // calling poll() without sleeping.
    //
    // These methods are OPTIONAL.
    // Advanced users may continue to drive poll() manually.
    //
    // Threading:
    //   - Not thread-safe
    //   - Must be called from the same thread as poll()
    //
    // -------------------------------------------------------------------------

    // -----------------------------------------------------------------------------
    // Convenience execution loop — run until protocol quiescence
    // -----------------------------------------------------------------------------
    //
    // Drives the client by repeatedly calling poll() until the client
    // reaches quiescence.
    //
    // This is a thin convenience wrapper over: poll() + is_idle()
    //
    // Semantics:
    //
    //   - No background threads
    //   - No hidden scheduling
    //   - No protocol inference
    //   - No user intent inference
    //
    // The client remains fully poll-driven.
    //
    // Exit condition:
    //
    // The loop exits when is_idle() becomes true, meaning that,
    // at the instant of observation:
    //
    // - The underlying Core Session has no pending protocol work
    //       (ACKs, rejections, replay, control messages)
    //
    // - Lite owns no active callbacks or dispatchable behavior
    //
    // - If poll() is never called again, no further user callbacks
    //   will be invoked and no protocol obligations remain outstanding
    //
    // Non-goals:
    //
    //   - This does NOT represent steady-state execution
    //   - This does NOT prevent future messages if polling continues
    //   - This does NOT imply the connection is closed
    //
    // -----------------------------------------------------------------------------
    void run_until_idle(std::chrono::milliseconds tick = std::chrono::milliseconds{10});

    // -----------------------------------------------------------------------------
    // Run loop with external stop intent
    // -----------------------------------------------------------------------------
    //
    // Executes poll() repeatedly while the user-provided condition returns true.
    //
    // Semantics:
    //
    // - Exit condition is owned exclusively by the caller
    // - No protocol quiescence is inferred
    // - No drain or cleanup is performed
    // - No background threads
    //
    // This method:
    //   - does NOT infer protocol state
    //   - does NOT observe is_idle()
    //   - does NOT perform draining or shutdown logic
    //
    // If the stop condition becomes false, the method returns immediately.
    // Any remaining protocol or callback work must be handled explicitly
    // by the caller (e.g. via run_until_idle()).
    //
    // -----------------------------------------------------------------------------
    template<class StopFn>
    void run_while(StopFn&& should_continue, std::chrono::milliseconds tick = std::chrono::milliseconds{10}) {
        const bool cooperative = (tick.count() > 0);
        // Loop while the user condition indicates to continue
        while (should_continue()) [[likely]] {
            poll();
            if (cooperative) [[likely]] {
                std::this_thread::sleep_for(tick);
            }
        }
    }

    // -----------------------------------------------------------------------------
    // Run loop until external stop condition becomes true
    // -----------------------------------------------------------------------------
    //
    // Executes poll() repeatedly until the user-provided stop condition
    // evaluates to true.
    //
    // Semantics:
    //
    // - Exit condition is owned exclusively by the caller
    // - No protocol quiescence is inferred
    // - No drain or cleanup is performed
    // - No background threads
    //
    // This method is intended for:
    //
    // - Signal-driven applications (Ctrl+C)
    // - Time-bounded execution
    // - Message-count-limited loops
    // - Applications with explicit lifecycle control
    //
    // If protocol cleanup is required, the caller must explicitly invoke
    // unsubscribe() and/or run_until_idle() after this method returns.
    //
    // -----------------------------------------------------------------------------
    template<class StopFn>
    void run_until(StopFn&& should_stop, std::chrono::milliseconds tick = std::chrono::milliseconds{10}) {
        const bool cooperative = (tick.count() > 0);
        // Loop until either stop intent is observed
        while (!should_stop()) [[likely]] {
            poll();
            if (cooperative) [[likely]] {
                std::this_thread::sleep_for(tick);
            }
        }
    }

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
