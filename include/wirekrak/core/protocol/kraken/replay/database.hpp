// ============================================================================
// Replay Database (Core Protocol Infrastructure)
// ============================================================================
//
// The Replay Database stores **acknowledged protocol intent** (subscription
// requests) so that they can be deterministically replayed after a transport
// reconnect.
//
// This component is strictly part of the **protocol-correctness layer**.
// It does NOT store user callbacks, data-plane behavior, or application logic.
// Its sole responsibility is to preserve and replay *what the exchange has
// previously acknowledged as valid intent*.
//
// ---------------------------------------------------------------------------
// Design goals
// ---------------------------------------------------------------------------
// • Protocol truth only
//     - Stores typed subscription requests (e.g. trade, book)
//     - Never stores callbacks or user behavior
//
// • ACK-driven correctness
//     - Only subscriptions that were acknowledged are replayed
//     - Rejected symbols are permanently removed
//
// • Deterministic replay
//     - Replay is triggered exclusively by a transport epoch change
//     - No speculative retries or inferred recovery
//
// • Symbol-level precision
//     - Subscriptions may contain multiple symbols
//     - Rejections and unsubscriptions operate at symbol granularity
//     - Empty subscriptions are removed automatically
//
// • Type-safe and allocation-stable
//     - One strongly-typed Table per channel
//     - Compile-time routing via `channel_of_v<RequestT>`
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
// Behavioral concerns belong in higher layers (e.g. Lite).
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
// • try_process_rejection(req_id, symbol)
//     Permanently removes rejected intent
//
// • take_subscriptions()
//     Transfers all replayable intent during reconnect
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
//

#pragma once

#include "wirekrak/core/protocol/kraken/replay/table.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/channel_traits.hpp"
#include "wirekrak/core/protocol/kraken/enums.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace replay {

// =============================================================
// Database
// --------
// Keeps copies of subscription requests,
// so they can be replayed after reconnect.
// Key features:
// - Type-safe: one Table per channel type
// - Uses compile-time routing via if constexpr
// =============================================================
class Database {
public:
    Database() = default;
    ~Database() = default;

    // Insert or update a subscription request
    template<class RequestT>
    inline void add(RequestT req) noexcept {
        subscription_table_for_<RequestT>().add(std::move(req));
    }

    // Removes symbols regardless of originating req_id,
    // matching Kraken unsubscribe semantics.
    template<class RequestT>
    inline void remove(RequestT req) noexcept {
        auto& table = subscription_table_for_<RequestT>();
        for (const auto& symbol : req.symbols) {
            table.erase_symbol(symbol);
        }
    }

    // Process a protocol rejection by req_id and symbol, removing any matching intent from the table.
    // Returns true if a matching subscription was found and updated, false otherwise.
    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        bool done = trade_.try_process_rejection(req_id, symbol);
        if (!done) {
            done = book_.try_process_rejection(req_id, symbol);
        }
        if (done) {
            WK_DEBUG("[REPLAY:DB] Processed rejection for symbol {" << symbol << "} (req_id=" << req_id << ")");
        }
        return done;
    }

    // Transfer all replayable subscriptions of a given type
    template<class RequestT>
    [[nodiscard]]
    inline std::vector<Subscription<RequestT>> take_subscriptions() noexcept {
        return subscription_table_for_<RequestT>().take_subscriptions();
    }

    // Clear all stored protocol intent
    inline void clear_all() noexcept {
        trade_.clear();
        book_.clear();
    }

private:
    Table<schema::trade::Subscribe> trade_;
    // Table<ticker::Subscribe> ticker_;
    Table<schema::book::Subscribe> book_;

private:
    // Helpers to get the correct handler table for a response type
    template<class RequestT>
    auto& subscription_table_for_() {
        if constexpr (channel_of_v<RequestT> == Channel::Trade) {
            return trade_;
        }
        // else if constexpr (channel_of_v<RequestT> == Channel::Ticker) {
        //     return ticker_;
        // }
        else if constexpr (channel_of_v<RequestT> == Channel::Book) {
            return book_;
        }
        else {
            static_assert(wirekrak::core::always_false<RequestT>, "Unsupported RequestT in subscription_table_for_()");
        }
    }

    template<class RequestT>
    const auto& subscription_table_for_() const {
        if constexpr (channel_of_v<RequestT> == Channel::Trade) {
            return trade_;
        }
        // else if constexpr (channel_of_v<RequestT> == Channel::Ticker) {
        //     return ticker_;
        //}
        else if constexpr (channel_of_v<RequestT> == Channel::Book) {
             return book_;
        }
        else {
            static_assert(wirekrak::core::always_false<RequestT>, "Unsupported RequestT in subscription_table_for_()");
        }
    }

};

} // namespace replay
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
