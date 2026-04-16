#pragma once

#include <unordered_set>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/symbol/intern.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/protocol/subscription/pending_requests.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/trap.hpp"


namespace wirekrak::core::protocol::subscription {

/*
===============================================================================
Idempotent Manager
===============================================================================

Tracks protocol lifecycle for a single subscription.

State model:

    active_symbols_
    pending_subscriptions_
    pending_unsubscriptions_

Invariants:
-----------
• A symbol may exist in exactly one of:
    - active_symbols_
    - pending_subscriptions_
    - pending_unsubscriptions_

• total_symbols() represents logical ownership:
      active + pending_subscribe

• Pending unsubscribe symbols are still logically active.

Design:
-------
• Idempotent at symbol level
• Safe under reconnect replay storms
• Replay DB must match total_symbols()
• No symbol duplication allowed
===============================================================================
*/

template<class RequestT>
class Manager {
public:
    explicit Manager()
    {}

    // ------------------------------------------------------------
    // Outbound registration
    // ------------------------------------------------------------

    [[nodiscard]]
    inline RequestSymbols register_subscription(RequestSymbols symbols, ctrl::req_id_t req_id) noexcept {

        std::size_t write = 0;
        RequestSymbolIds accepted;
        // Filter symbols according to current state
        for (std::size_t read = 0; read < symbols.size(); ++read) {
            const auto& symbol = symbols[read];
            SymbolId sid = intern_symbol(symbol);
            // Already active -> ignore
            if (active_symbols_.contains(sid)) {
                continue;
            }
            // Already pending subscribe -> ignore
            if (pending_subscriptions_.contains(sid)) {
                continue;
            }
            // Cancel pending unsubscribe
            if (pending_unsubscriptions_.contains(sid)) {
                if (pending_unsubscriptions_.remove(sid)) {
                    active_symbols_.insert(sid);
                }
                continue;
            }
            // Keep symbol (move forward if needed)
            if (write != read) {
                symbols[write] = std::move(symbols[read]);
            }
            accepted.push_back(sid);
            ++write;
        }
        // Truncate to accepted symbols and register pending subscribe
        symbols.erase(symbols.begin() + write, symbols.end());
        if (!symbols.empty()) {
            pending_subscriptions_.add(req_id, accepted);
        }

        return symbols;  // moved to caller
    }

    [[nodiscard]]
    RequestSymbols register_unsubscription(RequestSymbols symbols, ctrl::req_id_t req_id) noexcept {
        RequestSymbols cancelled;
        RequestSymbolIds filtered;

        for (const auto& symbol : symbols) {
            // 0) Get symbol ID
            SymbolId sid = intern_symbol(symbol);
            // 1️) If pending subscribe -> cancel it immediately
            if (pending_subscriptions_.contains(sid)) {
                if (pending_subscriptions_.remove(sid)) {
                    cancelled.push_back(symbol);
                }
                continue;
            }
            // 2️) If not active -> ignore
            if (!active_symbols_.contains(sid)) {
                continue;
            }

            // 3️) Already pending unsubscribe -> ignore
            if (pending_unsubscriptions_.contains(sid)) {
                continue;
            }

            filtered.push_back(sid);
        }

        if (!filtered.empty()) {
            pending_unsubscriptions_.add(req_id, filtered);
        }

        return cancelled;  // moved to caller
    }

    // ------------------------------------------------------------
    // ACK processing
    // ------------------------------------------------------------

    inline void process_subscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        SymbolId sid = intern_symbol(symbol);

        if (!pending_subscriptions_.contains(sid)) {
            return;
        }

        if (success) {
            confirm_subscription_(req_id, sid);
        } else {
            reject_subscription_(req_id, sid);
        }
    }

    inline void process_unsubscribe_ack(ctrl::req_id_t req_id,  Symbol symbol,  bool success) noexcept {
        SymbolId sid = intern_symbol(symbol);
        
        if (!pending_unsubscriptions_.contains(sid)) {
            return;
        }

        if (success) {
            confirm_unsubscription_(req_id, sid);
        } else {
            reject_unsubscription_(req_id, sid);
        }
    }

    // ------------------------------------------------------------
    // Rejection notice (generic path)
    // ------------------------------------------------------------

    [[nodiscard]]
    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        SymbolId sid = intern_symbol(symbol);
        
        if (pending_subscriptions_.remove(req_id, sid)) {
            return true;
        }

        if (pending_unsubscriptions_.remove(req_id, sid)) {
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------
    // Logical state queries
    // ------------------------------------------------------------

    [[nodiscard]]
    inline bool has_pending_requests() const noexcept {
        return !pending_subscriptions_.empty() || !pending_unsubscriptions_.empty();
    }

    [[nodiscard]]
    inline std::size_t pending_requests() const noexcept {
        return pending_subscriptions_.count() + pending_unsubscriptions_.count();
    }

    // Number of pending subscriptions not full ACKed yet (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_subscription_requests() const noexcept {
        return pending_subscriptions_.count();
    }

    // Number of pending unsubscriptions not full ACKed yet (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_unsubscription_requests() const noexcept {
        return pending_unsubscriptions_.count();
    }

    // Returns true if there is at least one fully active subscription
    [[nodiscard]]
    inline bool has_active_symbols() const noexcept {
        return !active_symbols_.empty();
    }

    // Number of active subscribed symbols (useful for debugging)
    [[nodiscard]]
    inline std::size_t active_symbols() const noexcept {
        return active_symbols_.size();
    }

    // Logical ownership view
    [[nodiscard]]
    inline std::size_t total_symbols() const noexcept {
        return active_symbols_.size() + pending_subscriptions_.symbol_count();
    }

    // Number of pending symbols awaiting ACK (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_symbols() const noexcept {
        return pending_subscriptions_.symbol_count() + pending_unsubscriptions_.symbol_count();
    }

    // Number of pending subscribed symbols awaiting ACK (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_subscribe_symbols() const noexcept {
        return pending_subscriptions_.symbol_count();
    }

    // Number of pending unsubscribed symbols awaiting ACK (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_unsubscribe_symbols() const noexcept {
        return pending_unsubscriptions_.symbol_count();
    }

    // ------------------------------------------------------------
    // Reset
    // ------------------------------------------------------------

    inline void clear_all() noexcept {
        pending_subscriptions_.clear();
        pending_unsubscriptions_.clear();
        active_symbols_.clear();
    }

#ifndef NDEBUG
    void assert_consistency() const {
        for (auto sid : active_symbols_) {
            LCR_ASSERT(!pending_subscriptions_.contains(sid));
            LCR_ASSERT(!pending_unsubscriptions_.contains(sid));
        }

        pending_subscriptions_.assert_consistency();
        pending_unsubscriptions_.assert_consistency();
    }
#endif

private:

    // ------------------------------------------------------------
    // Internal transitions
    // ------------------------------------------------------------

    inline void confirm_subscription_(ctrl::req_id_t req_id, SymbolId sid) noexcept {
         if (!pending_subscriptions_.remove(req_id, sid)) {
            return;
        }

        active_symbols_.insert(sid);
    }

    inline void reject_subscription_(ctrl::req_id_t req_id, SymbolId sid) noexcept {
        if (!pending_subscriptions_.remove(req_id, sid)) {
            return;
        }
    }

    inline void confirm_unsubscription_(ctrl::req_id_t req_id, SymbolId sid) noexcept {

        if (!pending_unsubscriptions_.remove(req_id, sid)) {
            return;
        }

        active_symbols_.erase(sid);
    }

    inline void reject_unsubscription_(ctrl::req_id_t req_id, SymbolId sid) noexcept {
        if (!pending_unsubscriptions_.remove(req_id, sid)) {
            return;
        }
    }

private:
    PendingRequests pending_subscriptions_;
    PendingRequests pending_unsubscriptions_;

    std::unordered_set<SymbolId> active_symbols_;
};

} // namespace wirekrak::core::protocol::subscription
