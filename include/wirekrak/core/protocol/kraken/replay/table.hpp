/*
===============================================================================
Replay Table<RequestT> (Protocol Intent Storage)
===============================================================================

A Replay Table stores **acknowledged subscription intent** for a single Kraken
channel type (e.g. trade, book), at **symbol granularity**, so that intent can be
deterministically replayed after a transport reconnect.

This is a low-level, protocol-facing container used exclusively by the
Replay Database and the Session. It does NOT contain user behavior or data-plane
logic.

-------------------------------------------------------------------------------
Responsibilities
-------------------------------------------------------------------------------
• Store fully-typed subscription requests (`RequestT`)
• Preserve request parameters exactly as acknowledged by the exchange
• Support symbol-level mutation due to:
    - explicit unsubscription
    - protocol rejection
• Remove subscriptions automatically when they become empty
• Provide replayable intent on reconnect

-------------------------------------------------------------------------------
Core invariants
-------------------------------------------------------------------------------
• Each entry represents ONE protocol request (identified by req_id)
• Each request may contain N symbols
• Symbols are removed individually, never partially replayed
• Empty subscriptions are erased eagerly
• Replay order is unspecified and protocol-safe

-------------------------------------------------------------------------------
What this class deliberately does NOT do
-------------------------------------------------------------------------------
• Does NOT store callbacks
• Does NOT dispatch messages
• Does NOT infer protocol state
• Does NOT retry or repair intent
• Does NOT perform I/O

-------------------------------------------------------------------------------
Rejection & unsubscribe semantics
-------------------------------------------------------------------------------
• `try_process_rejection(req_id, symbol)`
    Removes a rejected symbol from the matching request

• `erase_symbol(symbol)`
    Removes a symbol due to explicit unsubscribe, matching Kraken semantics:
    the symbol is removed from the first subscription that contains it

-------------------------------------------------------------------------------
Lifecycle
-------------------------------------------------------------------------------
• add(req)
    Store acknowledged subscription intent

• try_process_rejection(req_id, symbol)
    Apply permanent protocol rejection

• erase_symbol(symbol)
    Apply explicit unsubscribe

• take_subscriptions()
    Transfer all stored intent for replay after reconnect

• clear()
    Drop all stored intent (shutdown / reset)

-------------------------------------------------------------------------------
Threading & performance
-------------------------------------------------------------------------------
• Not thread-safe
• Owned by the Session event loop
• No blocking
• Allocation-stable after warm-up
• Linear scans are acceptable due to bounded subscription counts

===============================================================================
*/

#pragma once

#include <unordered_map>
#include <cstdint>
#include <utility>

#include "wirekrak/core/protocol/kraken/replay/subscription.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace replay {

/*
    Table<RequestT>
    -----------------------------
    Stores outbound subscription requests,
    allowing automatic replay after reconnect.

    Key features:
    - Type-safe: one DB per channel type (trade, ticker, book, …)
    - Stores a full request object (including symbols/settings)
    - Supports replay, removal, iteration, etc.
*/
template<class RequestT>
class Table {
public:
    Table() = default;

    // ------------------------------------------------------------
    // Add a new replay subscription
    // table_.emplace_back(request)
    // ------------------------------------------------------------
    inline void add(RequestT req) {
        if (!req.req_id.has()) {
            WK_FATAL("[REPLAY:TABLE] Attempted to add subscription with invalid req_id");
            return;
        }
        ctrl::req_id_t req_id = req.req_id.value();
        auto [it, inserted] = subscriptions_.emplace(req_id, Subscription<RequestT>{std::move(req)});
        if (!inserted) [[unlikely]] {
            WK_FATAL("[REPLAY:TABLE] Duplicate req_id detected: " << req_id);
        }
        WK_TRACE("[REPLAY:TABLE] Added subscription with req_id=" << req_id << " and " << it->second.request().symbols.size() << " symbol(s) " << " (total subscriptions=" << subscriptions_.size() << ")");
    }

    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        // Find the subscription matching the req_id
        auto it = subscriptions_.find(req_id);
        if (it == subscriptions_.end()) {
            return false;
        }
        // Apply rejection to the matching subscription
        bool done = it->second.try_process_rejection(req_id, symbol);
        if (done) {
            WK_TRACE("[REPLAY:TABLE] Rejected symbol {" << symbol << "} from subscription (req_id=" << req_id << ")");
            if (it->second.empty()) {
                subscriptions_.erase(it);
                WK_TRACE("[REPLAY:TABLE] Removed empty subscription with req_id=" << req_id << " (total subscriptions=" << subscriptions_.size() << ")");
            }
        }

        return done;
    }

    // ------------------------------------------------------------
    // Erase the first occurrence of a symbol from any subscription
    // This matches Kraken unsubscribe semantics:
    //   "unsubscribe(symbol)" removes that symbol from whichever
    //   subscription contains it.
    // ------------------------------------------------------------
    inline void erase_symbol(Symbol symbol) noexcept {
        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ) {
            auto& subscription = it->second;
            bool removed = subscription.erase_symbol(symbol);
            if (removed) {
                if (subscription.empty()) {
                    WK_TRACE("[REPLAY:TABLE] Removed empty subscription with req_id=" << subscription.req_id() << " (total subscriptions=" << subscriptions_.size() - 1 << ")");
                    it = subscriptions_.erase(it);
                }
                return;
            }
            ++it;
        }
        WK_WARN("[REPLAY:TABLE] Failed to erase symbol {" << symbol << "} from any subscription (not found)");
    }

    [[nodiscard]]
    inline bool contains_symbol(Symbol symbol) const noexcept {
        for (const auto& [_, sub] : subscriptions_) {
            if (sub.contains(symbol)) {
                return true;
            }
        }
        return false;
    }

    // ------------------------------------------------------------
    // Debug/utility
    // ------------------------------------------------------------

    [[nodiscard]]
    inline bool empty() const noexcept {
        return subscriptions_.empty();
    }

    [[nodiscard]]
    inline size_t size() const noexcept {
        return subscriptions_.size();
    }

    inline void clear() noexcept {
        subscriptions_.clear();
    }

    // for diagnostics only, not iteration-based logic, since order is unspecified
    [[nodiscard]]
    inline const auto& subscriptions() const noexcept {
        return subscriptions_;
    }

    [[nodiscard]]
    inline std::size_t total_requests() const noexcept {
        return subscriptions_.size();
    }

    [[nodiscard]]
    inline size_t total_symbols() const noexcept {
        size_t count = 0;
        for (const auto& [_, sub] : subscriptions_) {
            count += sub.request().symbols.size();
        }
        return count;
    }

    [[nodiscard]]
    inline std::vector<Subscription<RequestT>> take_subscriptions() noexcept {
        // Prepare output vector
        std::vector<Subscription<RequestT>> out;
        out.reserve(subscriptions_.size());
        // Move all subscriptions out of the table for replay
        for (auto& [_, sub] : subscriptions_) {
            out.push_back(std::move(sub));
        }
        // Clear the table after moving out subscriptions
        subscriptions_.clear();
        return out;
    }

private:
    std::unordered_map<ctrl::req_id_t, Subscription<RequestT>> subscriptions_;
};

} // namespace replay
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
