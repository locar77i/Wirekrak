#pragma once

#include <unordered_set>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/protocol/kraken/enums.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/symbol/intern.hpp"
#include "wirekrak/core/protocol/kraken/channel/pending_requests.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core::protocol::kraken::channel {

/*
===============================================================================
Idempotent Manager
===============================================================================

Tracks protocol subscription lifecycle for a single channel.

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

class Manager {
public:
    explicit Manager(Channel channel)
        : channel_(channel)
    {}

    // ------------------------------------------------------------
    // Outbound registration
    // ------------------------------------------------------------

    [[nodiscard]]
    inline std::vector<Symbol> register_subscription(std::vector<Symbol> symbols, ctrl::req_id_t req_id) noexcept {
        WK_TRACE("[SUBMGR:" << to_string(channel_) << "] Registering subscription request (req_id=" << req_id << ")");

        std::size_t write = 0;
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
                if (pending_unsubscriptions_.remove_symbol(symbol)) {
                    active_symbols_.insert(sid);
                }
                continue;
            }
            // Keep symbol (move forward if needed)
            if (write != read) {
                symbols[write] = std::move(symbols[read]);
            }
            ++write;
        }
        // Resize to filtered size and register pending subscribe
        symbols.resize(write);
        if (!symbols.empty()) {
            pending_subscriptions_.add(req_id, symbols);
        }

        log_state_();

        return symbols;  // moved to caller
    }

    [[nodiscard]]
    std::vector<Symbol> register_unsubscription(std::vector<Symbol> symbols, ctrl::req_id_t req_id) noexcept {
        WK_TRACE("[SUBMGR:" << to_string(channel_) << "] Registering unsubscription request (req_id=" << req_id << ")");
        std::vector<Symbol> filtered;
        filtered.reserve(symbols.size());

        std::vector<Symbol> cancelled;

        for (const auto& symbol : symbols) {
            // 0) Get symbol ID
            SymbolId sid = intern_symbol(symbol);
            // 1️) If pending subscribe -> cancel it immediately
            if (pending_subscriptions_.contains(sid)) {
                WK_TRACE("[SUBMGR:" << to_string(channel_) << "] Cancelling pending subscription for symbol {" << symbol << "} (req_id=" << req_id << ")");
                if (pending_subscriptions_.remove_symbol(symbol)) {
                    cancelled.push_back(symbol);
                }
                continue;
            }
            // 2️) If not active -> ignore
            if (!active_symbols_.contains(sid)) {
                WK_TRACE("[SUBMGR:" << to_string(channel_) << "] Ignoring unsubscription for non-active symbol {" << symbol << "} (req_id=" << req_id << ")");
                continue;
            }

            // 3️) Already pending unsubscribe -> ignore
            if (pending_unsubscriptions_.contains(sid)) {
                WK_TRACE("[SUBMGR:" << to_string(channel_) << "] Ignoring unsubscription for already pending symbol {" << symbol << "} (req_id=" << req_id << ")");
                continue;
            }

            filtered.push_back(symbol);
        }

        if (!filtered.empty()) {
            pending_unsubscriptions_.add(req_id, filtered);
        }

        log_state_();
        return cancelled;  // moved to caller
    }

    // ------------------------------------------------------------
    // ACK processing
    // ------------------------------------------------------------

    inline void process_subscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        WK_TRACE("[SUBMGR:" << to_string(channel_) << "] Processing subscribe ACK for symbol {" << symbol << "} (req_id=" << req_id << ") - success=" << std::boolalpha << success);
        if (!pending_subscriptions_.contains(symbol)) {
            WK_WARN("[SUBMGR:" << to_string(channel_) << "] Subscription OMITTED for symbol {" << symbol << "} (unknown req_id=" << req_id << ")");
            return;
        }

        if (success) {
            confirm_subscription_(req_id, symbol);
        } else {
            reject_subscription_(req_id, symbol);
        }

        log_state_();
    }

    inline void process_unsubscribe_ack(ctrl::req_id_t req_id,  Symbol symbol,  bool success) noexcept {
        WK_TRACE("[SUBMGR:" << to_string(channel_) << "] Processing unsubscribe ACK for symbol {" << symbol << "} (req_id=" << req_id << ") - success=" << std::boolalpha << success);
        if (!pending_unsubscriptions_.contains(symbol)) {
            WK_WARN("[SUBMGR:" << to_string(channel_) << "] Unsubscription OMITTED for symbol {" << symbol << "} (unknown req_id=" << req_id << ")");
            return;
        }

        if (success) {
            confirm_unsubscription_(req_id, symbol);
        } else {
            reject_unsubscription_(req_id, symbol);
        }

        log_state_();
    }

    // ------------------------------------------------------------
    // Rejection notice (generic path)
    // ------------------------------------------------------------

    [[nodiscard]]
    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        if (pending_subscriptions_.remove(req_id, symbol)) {
            WK_WARN("[SUBMGR:" << to_string(channel_) << "] Subscription REJECTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
            return true;
        }

        if (pending_unsubscriptions_.remove(req_id, symbol)) {
            WK_WARN("[SUBMGR:" << to_string(channel_) << "] Unsubscription REJECTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
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
            assert(!pending_subscriptions_.contains(sid));
            assert(!pending_unsubscriptions_.contains(sid));
        }

        pending_subscriptions_.assert_consistency();
        pending_unsubscriptions_.assert_consistency();
    }
#endif

private:

    // ------------------------------------------------------------
    // Internal transitions
    // ------------------------------------------------------------

    inline void confirm_subscription_(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        WK_DEBUG("[SUBMGR:" << to_string(channel_) << "] Confirming subscription for symbol {" << symbol << "} (req_id=" << req_id << ")");
        SymbolId sid = intern_symbol(symbol);

        if (!pending_subscriptions_.remove(req_id, symbol)) {
            WK_TRACE("[SUBMGR:" << to_string(channel_) << "] No matching pending request for symbol {" << symbol << "} (req_id=" << req_id << ") - Ignoring");
            return;
        }

        active_symbols_.insert(sid);
        WK_INFO("[SUBMGR:" << to_string(channel_) << "] Subscription ACCEPTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
    }

    inline void reject_subscription_(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        WK_DEBUG("[SUBMGR:" << to_string(channel_) << "] Rejecting subscription for symbol {" << symbol << "} (req_id=" << req_id << ")");
        if (!pending_subscriptions_.remove(req_id, symbol)) {
            WK_TRACE("[SUBMGR:" << to_string(channel_) << "] No matching pending request for symbol {" << symbol << "} (req_id=" << req_id << ") - Ignoring");
            return;
        }
        WK_WARN("[SUBMGR:" << to_string(channel_) << "] Subscription REJECTED for symbol  {" << symbol << "} (req_id=" << req_id << ")");
    }

    inline void confirm_unsubscription_(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        WK_DEBUG("[SUBMGR:" << to_string(channel_) << "] Confirming unsubscription for symbol {" << symbol << "} (req_id=" << req_id << ")");
        SymbolId sid = intern_symbol(symbol);

        if (!pending_unsubscriptions_.remove(req_id, symbol)) {
            WK_TRACE("[SUBMGR:" << to_string(channel_) << "] No matching pending request for symbol {" << symbol << "} (req_id=" << req_id << ") - Ignoring");
            return;
        }

        active_symbols_.erase(sid);
        WK_INFO("[SUBMGR:" << to_string(channel_) << "] Unsubscription ACCEPTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
    }

    inline void reject_unsubscription_(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        WK_DEBUG("[SUBMGR:" << to_string(channel_) << "] Rejecting unsubscription for symbol {" << symbol << "} (req_id=" << req_id << ")");
        if (!pending_unsubscriptions_.remove(req_id, symbol)) {
            WK_TRACE("[SUBMGR:" << to_string(channel_) << "] No matching pending request for symbol {" << symbol << "} (req_id=" << req_id << ") - Ignoring");
            return;
        }
        WK_WARN("[SUBMGR:" << to_string(channel_) << "] Unsubscription REJECTED for symbol  {" << symbol << "} (req_id=" << req_id << ")");
    }

    inline void log_state_() const noexcept {
        WK_DEBUG("[SUBMGR:" << to_string(channel_) << "] Active subscriptions = " << active_symbols_.size()
            << " - Pending subscriptions = " << pending_subscriptions_.symbol_count()
            << " - Pending unsubscriptions = " << pending_unsubscriptions_.symbol_count()
        );
    }

private:
    Channel channel_;

    PendingRequests pending_subscriptions_;
    PendingRequests pending_unsubscriptions_;

    std::unordered_set<SymbolId> active_symbols_;
};

} // namespace wirekrak::core::protocol::kraken::channel
