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
#include "wirekrak/core/symbol/intern.hpp"
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
    inline bool add(RequestT req) {
        // 0) Validate req_id presence
        if (!req.req_id.has()) {
            WK_FATAL("[REPLAY:TABLE] Attempted to add subscription with invalid req_id");
            return false;
        }
        ctrl::req_id_t req_id = req.req_id.value();
        // 1) Enforce symbol uniqueness
        // ------------------------------------------------------------
        // FIRST-WRITE-WINS POLICY:
        // Remove duplicated symbols from incoming request.
        // Do NOT mutate existing subscriptions.
        // ------------------------------------------------------------
        auto& symbols = req.symbols;
        symbols.erase(
            std::remove_if(
                symbols.begin(),
                symbols.end(),
                [&](const Symbol& symbol) {
                    auto simbol_id = intern_symbol(symbol);
                    auto it = symbol_owner_.find(simbol_id);
                    if (it != symbol_owner_.end()) {
                        WK_TRACE("[REPLAY:TABLE] Ignoring duplicate symbol {" << symbol << "} already owned by req_id=" << it->second);
                        return true; // drop from incoming request
                    }
                    return false;
                }
            ),
            symbols.end()
        );
        // 2) If nothing left after filtering, ignore entire request
        if (symbols.empty()) {
            WK_TRACE("[REPLAY:TABLE] Dropping empty subscription request (all symbols duplicated) req_id=" << req_id);
            return false;
        }
        // 3) Insert subscription into the table
        auto [it, inserted] = subscriptions_.emplace(req_id, Subscription<RequestT>{std::move(req)});
        if (!inserted) [[unlikely]] {
            WK_FATAL("[REPLAY:TABLE] Duplicate req_id detected: " << req_id);
            return false; // hard stop to protect invariants
        }
        // 4) Register ownership after successful insertion
        for (const auto& symbol : it->second.request().symbols) {
            SymbolId sid = intern_symbol(symbol);
            symbol_owner_.emplace(sid, req_id);
        }
        WK_TRACE("[REPLAY:TABLE] Added subscription with req_id=" << req_id << " and " << it->second.request().symbols.size() << " symbol(s) " << " (total subscriptions=" << subscriptions_.size() << ")");
        return true;
    }

    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        // 1) Find the subscription matching the req_id
        auto it = subscriptions_.find(req_id);
        if (it == subscriptions_.end()) {
            return false;
        }
        // 2) Apply rejection to the matching subscription
        bool done = it->second.try_process_rejection(req_id, symbol);
        if (!done) {
            return false;
        }
        WK_TRACE("[REPLAY:TABLE] Rejected symbol {" << symbol << "} from subscription (req_id=" << req_id << ")");
        // 3) Remove the symbol track from the ownership map
        SymbolId sid = intern_symbol(symbol);
        symbol_owner_.erase(sid);
        // 4) If the subscription is now empty, remove it from the table
        if (it->second.empty()) {
            subscriptions_.erase(it);
            WK_TRACE("[REPLAY:TABLE] Removed empty subscription with req_id=" << req_id << " (total subscriptions=" << subscriptions_.size() << ")");
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
        // 1) Find the req_id owning the symbol
        SymbolId sid = intern_symbol(symbol);
        auto owner_it = symbol_owner_.find(sid);
        if (owner_it == symbol_owner_.end()) {
            WK_WARN("[REPLAY:TABLE] Symbol {" << symbol << "} not found in the ownership map (cannot erase)");
            return;
        }
        ctrl::req_id_t req_id = owner_it->second;
        // 2) Find the subscription by req_id
        auto sub_it = subscriptions_.find(req_id);
        if (sub_it == subscriptions_.end()) {
            WK_WARN("[REPLAY:TABLE] Symbol {" << symbol << "} has req_id=" << req_id << " but no matching subscription found (inconsistent state)");
            symbol_owner_.erase(owner_it);
            return;
        }
        // 3) Remove the symbol from the subscription
        bool removed = sub_it->second.erase_symbol(symbol);
        if (removed) {
            // 4) Remove the symbol track from the ownership map
            symbol_owner_.erase(owner_it);
            // 5) If the subscription is now empty, remove it from the table
            if (sub_it->second.empty()) {
                subscriptions_.erase(sub_it);
                WK_TRACE("[REPLAY:TABLE] Removed empty subscription (req_id=" << req_id << ")");
            }
        }
    }

    [[nodiscard]]
    inline bool contains_symbol(Symbol symbol) const noexcept {
        SymbolId sid = intern_symbol(symbol);
        return symbol_owner_.find(sid) != symbol_owner_.end();
    }

    // ------------------------------------------------------------
    // Debug/utility
    // ------------------------------------------------------------

    [[nodiscard]]
    inline bool empty() const noexcept {
        return subscriptions_.empty();
    }

    inline void clear() noexcept {
        subscriptions_.clear();
        symbol_owner_.clear();
    }

    [[nodiscard]]
    inline std::size_t total_requests() const noexcept {
        return subscriptions_.size();
    }

    [[nodiscard]]
    inline size_t total_symbols() const noexcept {
        return symbol_owner_.size();
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
        // Clear table state after moving out subscriptions
        clear();
        return out;
    }

#ifndef NDEBUG
    void assert_consistency() const {
        size_t symbol_count = 0;
        for (const auto& [_, sub] : subscriptions_) {
            symbol_count += sub.request().symbols.size();
        }
        assert(symbol_count == symbol_owner_.size());
    }
#endif

private:
    std::unordered_map<ctrl::req_id_t, Subscription<RequestT>> subscriptions_;
    std::unordered_map<SymbolId, ctrl::req_id_t> symbol_owner_;
};

} // namespace replay
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
