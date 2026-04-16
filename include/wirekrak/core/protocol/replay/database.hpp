#pragma once

// ============================================================================
// Replay Database (Generic Protocol Infrastructure)
// ============================================================================
//
// The Replay Database stores **acknowledged protocol intent** (subscription
// state) in a fully generic, type-safe manner, enabling deterministic replay
// after transport reconnects.
//
// Unlike protocol-specific implementations, this database is **exchange-agnostic**
// and relies entirely on compile-time type information to manage subscription
// state.
//
// This component is part of the **protocol correctness layer**.
// It does NOT store user callbacks, data-plane behavior, or application logic.
// Its sole responsibility is to preserve and replay *what the exchange has
// acknowledged as valid intent*.
//
// ---------------------------------------------------------------------------
// Design goals
// ---------------------------------------------------------------------------
// • Protocol truth only
//     - Stores persistent subscription intent (via Table<RequestT>)
//     - Does not store user behavior or callbacks
//
// • Generic and extensible
//     - Supports any protocol via template parameter pack (SubscriptionTs...)
//     - No exchange-specific logic or branching
//
// • Compile-time routing
//     - One Table per subscription type
//     - Dispatch resolved entirely at compile time (no virtuals, no maps)
//
// • Deterministic replay
//     - Replay triggered exclusively by transport lifecycle events
//     - No speculative retries or inferred recovery
//
// • Symbol-level precision
//     - Subscriptions may contain multiple symbols
//     - Unsubscriptions and rejections operate at symbol granularity
//     - Empty subscriptions are removed automatically
//
// • Allocation-stable and low-latency
//     - No dynamic dispatch
//     - No hidden allocations in hot paths
//
// ---------------------------------------------------------------------------
// Relationship with subscription_traits
// ---------------------------------------------------------------------------
// • The database operates on **subscription types only**
// • Mapping from RequestT → subscription type is defined externally via
//   subscription_traits
//
// Example:
//     trade::Subscribe   → stored directly
//     trade::Unsubscribe → mapped to trade::Subscribe (via subscription_traits)
//
// This separation ensures:
//     - Database remains protocol-agnostic
//     - Protocol-specific logic stays in traits
//
// ---------------------------------------------------------------------------
// What this is NOT
// ---------------------------------------------------------------------------
// • Not a dispatcher
// • Not a callback registry
// • Not a data-plane buffer
// • Not a subscription manager
//
// The Replay Database preserves *protocol intent only*.
// Behavioral concerns belong in higher layers.
//
// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
// • add(req)
//     Records acknowledged subscription intent
//
// • remove(req)
//     Removes symbols due to explicit unsubscribe
//
// • remove_symbol(symbol)
//     Removes a single symbol (ACK-driven removal)
//
// • try_process_rejection(req_id, symbol)
//     Removes rejected intent based on protocol feedback
//
// • take_subscriptions<RequestT>()
//     Transfers all replayable subscriptions of a given type
//
// • clear_all()
//     Drops all stored protocol intent (e.g. shutdown)
//
// ---------------------------------------------------------------------------
// Threading & usage
// ---------------------------------------------------------------------------
// • Owned and used exclusively by the Session event loop
// • Not thread-safe
// • No blocking, no allocation on hot paths
//
// ============================================================================

#include <tuple>
#include <vector>
#include <utility>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/protocol/replay/table.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/protocol/subscription_traits.hpp"
#include "lcr/log/logger.hpp"

namespace wirekrak::core::protocol::replay {

// =============================================================
// Generic Replay Database
// -------------------------------------------------------------
// Template-based database storing one Table<RequestT> per request type.
//
// Key properties:
// - Fully generic (no exchange-specific logic)
// - Compile-time dispatch via type system
// - Zero runtime overhead
// - Preserves all original semantics
// =============================================================
template<class... SubscriptionTs>
class Database {
public:
    Database() = default;
    ~Database() = default;

    // ------------------------------------------------------------
    // Insert or update a subscription request
    // ------------------------------------------------------------
    template<class RequestT>
    inline void add(RequestT req) noexcept {
        table_<subscription_type<RequestT>>().add(std::move(req));
    }

    // ------------------------------------------------------------
    // Removes symbols (unsubscribe semantics)
    // ------------------------------------------------------------
    template<class RequestT>
    inline void remove(RequestT req) noexcept {
        auto& t = table_<subscription_type<RequestT>>();
        for (const auto& symbol : req.symbols) {
            t.erase_symbol(symbol);
        }
    }

    // ------------------------------------------------------------
    // Remove a single symbol (ACK-driven removal)
    // ------------------------------------------------------------
    template<class RequestT>
    inline void remove_symbol(Symbol symbol) noexcept {
        table_<subscription_type<RequestT>>().erase_symbol(symbol);
    }

    // ------------------------------------------------------------
    // Process rejection across all tables
    // ------------------------------------------------------------
    [[nodiscard]]
    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        bool done = false;

        std::apply([&](auto&... t) {
            ((done = done || t.try_process_rejection(req_id, symbol)), ...);
        }, tables_);

        if (done) {
            WK_DEBUG("[REPLAY:DB] Processed rejection for symbol {" << symbol << "} (req_id=" << req_id << ")");
        }

        return done;
    }

    // ------------------------------------------------------------
    // Transfer all replayable subscriptions for a given type
    // ------------------------------------------------------------
    template<class RequestT>
    [[nodiscard]]
    inline std::vector<Subscription<RequestT>> take_subscriptions() noexcept {
        return table_<subscription_type<RequestT>>().take_subscriptions();
    }

    // ------------------------------------------------------------
    // Clear all stored protocol intent
    // ------------------------------------------------------------
    inline void clear_all() noexcept {
        std::apply([](auto&... t) {
            (t.clear(), ...);
        }, tables_);
    }

    // ------------------------------------------------------------
    // Diagnostics
    // ------------------------------------------------------------

    [[nodiscard]]
    inline std::size_t total_requests() const noexcept {
        std::size_t total = 0;
        std::apply([&](const auto&... t) {
            ((total += t.total_requests()), ...);
        }, tables_);
        return total;
    }

    [[nodiscard]]
    inline std::size_t total_symbols() const noexcept {
        std::size_t total = 0;
        std::apply([&](const auto&... t) {
            ((total += t.total_symbols()), ...);
        }, tables_);
        return total;
    }

    // ------------------------------------------------------------
    // Access the Table associated with a given RequestT
    // (resolved via subscription_traits)
    // ------------------------------------------------------------
    template<class RequestT>
    auto& table_for() noexcept {
        return table_<subscription_type<RequestT>>();
    }

    template<class RequestT>
    const auto& table_for() const noexcept {
        return table_<subscription_type<RequestT>>();
    }

    // ------------------------------------------------------------
    // Iterate over all subscription types
    // ------------------------------------------------------------
    //
    // Invokes a callable once per subscription type (SubscriptionT)
    //
    // The callable must support a templated call operator:
    //
    //   db.for_each([&]<class T>() {
    //       // use T
    //   });
    //
    // This enables fully generic replay logic without
    // exposing internal storage or requiring runtime dispatch.
    //
    // ------------------------------------------------------------
    template<class F>
    inline void for_each(F&& f) noexcept {
        (f.template operator()<SubscriptionTs>(), ...);
    }

    template<class F>
    inline void for_each(F&& f) const noexcept {
        (f.template operator()<SubscriptionTs>(), ...);
    }

    // ------------------------------------------------------------
    // Iterate over all subscription tables
    // ------------------------------------------------------------
    //
    // Similar to for_each, but provides direct access to each Table<SubscriptionT>.
    //
    //   db.for_each_table([&]<class T>(Table<T>& t) {
    //       // use T and t
    //   });
    //
    // This is useful for operations that need to inspect or modify the tables directly.
    //
    // ------------------------------------------------------------
    template<class F>
    inline void for_each_table(F&& f) noexcept {
        (f.template operator()<SubscriptionTs>(table_<SubscriptionTs>()), ...);
    }

    template<class F>
    inline void for_each_table(F&& f) const noexcept {
        (f.template operator()<SubscriptionTs>(table_<SubscriptionTs>()), ...);
    }

private:
    // ------------------------------------------------------------
    // Storage: one table per RequestT
    // ------------------------------------------------------------
    std::tuple<Table<SubscriptionTs>...> tables_;

    // ------------------------------------------------------------
    // Access helpers
    // ------------------------------------------------------------
    template<class RequestT>
    inline auto& table_() noexcept {
        static_assert((std::is_same_v<RequestT, SubscriptionTs> || ...), "Database does not manage this subscription type");
        return std::get<Table<RequestT>>(tables_);
    }

    template<class RequestT>
    inline const auto& table_() const noexcept {
        static_assert((std::is_same_v<RequestT, SubscriptionTs> || ...), "Database does not manage this subscription type");
        return std::get<Table<RequestT>>(tables_);
    }

    
};

} // namespace wirekrak::core::protocol::replay
