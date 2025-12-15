#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/types.hpp"
#include "wirekrak/core/symbol/intern.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak {
namespace channel {

/*
  Manager
   --------------------
   Tracks all outbound subscribe/unsubscribe requests and transitions:
  
     (initial state)
         ↓ (on subscribe request)
     pending_subscriptions_ (waiting for ACK)
         ↓ (on ACK)
     active_  (inserted)
         ↓ (on unsubscribe request)
     pending_unsubscriptions_ (waiting for ACK)
         ↓ (on ACK)
     active_  (removed)

   On reconnect(), only active subscriptions are automatically replayed.
 */
class Manager {
public:

    class SymbolGroup {
    public:
        struct SymbolEntry {
            SymbolId symbol_id;
            uint64_t group_id;  // the req_id of the original subscription request
        };

    private:
        std::vector<SymbolEntry> entries_;
    
    public:
        [[nodiscard]]
        inline std::vector<SymbolEntry>& entries() noexcept {
            return entries_;
        }

        [[nodiscard]]
        inline const std::vector<SymbolEntry>& entries() const noexcept {
            return entries_;
        }

        [[nodiscard]]
        inline bool empty() const noexcept {
            return entries_.empty();
        }

        [[nodiscard]]
        inline std::size_t size() const noexcept {
            return entries_.size();
        }

        inline void erase(SymbolId symbol_id) noexcept {
            auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const SymbolEntry& e) { return e.symbol_id == symbol_id; });
            if (it != entries_.end()) {
                entries_.erase(it, entries_.end());
            }
        }

        inline bool contains(SymbolId id) const noexcept {
            return std::any_of(entries_.begin(), entries_.end(), [&](const SymbolEntry& e){ return e.symbol_id == id; });
        }

        
    };

public:
    Manager() = default;

    // Register outbound subscribe request (called before sending a subscribe request)
    inline void register_subscription(std::vector<Symbol> symbols, std::uint64_t req_id) noexcept {
        // Store pending symbols
        auto& vec = pending_subscriptions_[req_id];
        vec.reserve(symbols.size());
        for (auto& s : symbols) {
            vec.push_back(intern_symbol(s));
        }
    }
    // Register outbound unsubscribe request (called before sending an unsubscribe request)
    inline void register_unsubscription(std::vector<Symbol> symbols, std::uint64_t req_id) noexcept {
        auto& vec = pending_unsubscriptions_[req_id];
        vec.reserve(symbols.size());
        for (auto& s : symbols) {
            vec.push_back(intern_symbol(s));
        }
    }

    // ------------------------------------------------------------
    // Process ACK messages
    // ------------------------------------------------------------

    inline void process_subscribe_ack(std::uint64_t group_id, Symbol symbol, bool success) noexcept {
        bool done = false;
        if (success) [[likely]] {
            done = confirm_subscription_(symbol, group_id);
            if (done) WK_INFO("[SUBMGR] Subscription ACCEPTED for channel 'trade' {" << symbol << "} (req_id=" << group_id << ")");
        }
        else {
            done = reject_subscription_(symbol, group_id);
            if (done) WK_WARN("[SUBMGR] Subscription REJECTED for channel 'trade'  {" << symbol << "} (req_id=" << group_id << ")");
        }
        if (!done) WK_WARN("[SUBMGR] Subscription OMITTED for channel 'trade' {" << symbol << "} (unknown req_id=" << group_id << ")");
    }

    inline void process_unsubscribe_ack(std::uint64_t group_id, Symbol symbol, bool success) noexcept {
        bool done = false;
        if (success) [[likely]] {
            done = confirm_unsubscription_(symbol, group_id);
            if (done) WK_INFO("[SUBMGR] Unsubscription ACCEPTED for channel 'trade' {" << symbol << "} (req_id=" << group_id << ")");
        }
        else {
            done = reject_unsubscription_(symbol, group_id);
            if (done) WK_WARN("[SUBMGR] Unsubscription REJECTED for channel 'trade' {" << symbol << "} (req_id=" << group_id << ")");
        }
        if (!done) WK_WARN("[SUBMGR] Unsubscription ACK omitted for channel 'trade' {" << symbol << "} (unknown req_id=" << group_id << ")");
    }

    // ------------------------------------------------------------
    // State queries
    // ------------------------------------------------------------

    // Returns true if any request has not been ACKed yet
    [[nodiscard]] bool has_pending() const noexcept {
        return !(pending_subscriptions_.empty() && pending_unsubscriptions_.empty());
    }

    // Number of pending requests (useful for debugging)
    [[nodiscard]] std::size_t pending_total() const noexcept {
        return pending_subscriptions_.size() + pending_unsubscriptions_.size();
    }

    // Returns true if there is at least one fully active subscription
    [[nodiscard]] bool has_active() const noexcept {
        return !active_.empty();
    }

    // Number of active subscriptions
    [[nodiscard]] std::size_t active_total() const noexcept {
        return active_.size();
    }

    // Access active subscriptions
    [[nodiscard]] inline const std::unordered_map<uint64_t, SymbolGroup>& active() const noexcept {
        return active_;
    }

    // ------------------------------------------------------------
    // Reset behavior
    // ------------------------------------------------------------

    // Drop pending subscriptions on reconnect
    inline void clear_pending() noexcept {
        pending_subscriptions_.clear();
        pending_unsubscriptions_.clear();
    }

    // Full reset (e.g., on shutdown, or full reconnect)
    inline void clear_all() noexcept {
        pending_subscriptions_.clear();
        pending_unsubscriptions_.clear();
        active_.clear();
    }

private:
    std::unordered_map<std::uint64_t, std::vector<SymbolId>> pending_subscriptions_;
    std::unordered_map<std::uint64_t, std::vector<SymbolId>> pending_unsubscriptions_;
    std::unordered_map<uint64_t, SymbolGroup> active_;

private:
    // ------------------------------------------------------------
    // Subscribe handling
    // ------------------------------------------------------------

    // Process ACK for subscription. Called when we parse a successful {"method":"subscribe","success":true}
    [[nodiscard]] inline bool confirm_subscription_(const Symbol& symbol, std::uint64_t req_id) noexcept {
        SymbolId symbol_id = intern_symbol(symbol);
        // Find pending subscription by req_id
        auto it = pending_subscriptions_.find(req_id);
        if (it == pending_subscriptions_.end()) {
            WK_WARN("[SUBMGR] Unable to confirm subscription - unknown req_id=" << req_id);
            return false;
        }
        auto& vec = it->second;
        // erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            WK_WARN("[SUBMGR] Unable to confirm subscription - symbol not found in pending (req_id=" << req_id << ")");
            return false;
        }
        // create active group now (if not exists)
        auto& group = active_[req_id];  // create or reuse the group
        // and an active entry immediately
        group.entries().push_back({
            .symbol_id = symbol_id,
            .group_id = req_id
        });
        // Erase symbol from pending list. If no symbols left → remove req_id entry
        vec.erase(pos);
        if (vec.empty()) {
            pending_subscriptions_.erase(it);
        }

        return true;
    }

    // Process ACK for subscription. Called when a subscribe request returns success=false
    [[nodiscard]] inline bool reject_subscription_(const Symbol& symbol, std::uint64_t req_id) noexcept {
        SymbolId symbol_id = intern_symbol(symbol);
        // Find pending subscription by req_id
        auto it = pending_subscriptions_.find(req_id);
        if (it == pending_subscriptions_.end()) {
            WK_WARN("[SUBMGR] Unable to reject subscription - no such pending request (req_id=" << req_id << ")");
            return false; // no such pending request
        }
        auto& vec = it->second;
        // erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            WK_WARN("[SUBMGR] Unable to reject subscription - symbol not found in pending (req_id=" << req_id << ")");
            return false;
        }
        vec.erase(pos);
         // If no symbols left → remove req_id entry
        if (vec.empty()) {
            pending_subscriptions_.erase(it);
        }
        return true;
    }

    // ------------------------------------------------------------
    // Unsubscribe handling
    // ------------------------------------------------------------

    // Process ACK for unsubscription. Called when we parse a successful {"method":"unsubscribe","success":true}
    [[nodiscard]] inline bool confirm_unsubscription_(const Symbol& symbol, std::uint64_t req_id) noexcept {
        SymbolId symbol_id = intern_symbol(symbol);
        // Find pending subscription by req_id
        auto it = pending_unsubscriptions_.find(req_id);
        if (it == pending_unsubscriptions_.end()) {
            WK_WARN("[SUBMGR] Unable to confirm unsubscription - unknown req_id=" << req_id);
            return false;
        }
        auto& vec = it->second;
        // erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            WK_WARN("[SUBMGR] Unable to confirm unsubscription - symbol not found in pending (req_id=" << req_id << ")");
            return false;
        }
        // Remove from active_ list
        auto active_it = active_.find(req_id);
        if (active_it != active_.end()) {
            auto& group = active_it->second;
            group.erase(symbol_id);
            if (group.empty()) { // If the subscription group is now empty → delete it
                active_.erase(active_it);
            }
        }
        // Erase symbol from pending list. If no symbols left → remove req_id entry
        vec.erase(pos);
        if (vec.empty()) {
            pending_unsubscriptions_.erase(it);
        }

        return true;
    }

    // Process ACK for unsubscription. Called when a subscribe request returns success=false
    [[nodiscard]] inline bool reject_unsubscription_(const Symbol& symbol, std::uint64_t req_id) noexcept {
        SymbolId symbol_id = intern_symbol(symbol);
        // Find pending subscription by req_id
        auto it = pending_unsubscriptions_.find(req_id);
        if (it == pending_unsubscriptions_.end()) {
            WK_WARN("[SUBMGR] Unable to reject unsubscription - no such pending request (req_id=" << req_id << ")");
            return false; // no such pending request
        }
        auto& vec = it->second;
        // erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            WK_WARN("[SUBMGR] Unable to reject unsubscription - symbol not found in pending (req_id=" << req_id << ")");
            return false;
        }
        vec.erase(pos);
         // If no symbols left → remove req_id entry
        if (vec.empty()) {
            pending_unsubscriptions_.erase(it);
        }
        return true;
    }

};

} // namespace channel
} // namespace wirekrak
