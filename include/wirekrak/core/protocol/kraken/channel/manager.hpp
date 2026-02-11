#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/protocol/kraken/enums.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/symbol/intern.hpp"
#include "lcr/log/logger.hpp"

namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace channel {

/*
  Manager
   --------------------
   Tracks all outbound subscribe/unsubscribe requests and transitions:
  
     (initial state)
         ↓ (on subscribe request)
     pending_subscriptions_ (waiting for ACK)
         ↓ (on ACK)
     active_symbols_  (inserted)
         ↓ (on unsubscribe request)
     pending_unsubscriptions_ (waiting for ACK)
         ↓ (on ACK)
     active_symbols_  (removed)

   On reconnect(), only active subscriptions are automatically replayed.
 */
class Manager {
public:
    Manager(Channel channel)
        : channel_(channel)
    {}

    // Register outbound subscribe request (called before sending a subscribe request)
    inline void register_subscription(std::vector<Symbol> symbols, ctrl::req_id_t req_id) noexcept {
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Registering subscription request (req_id=" << req_id << ")");
        // Store pending symbols
        auto& vec = pending_subscriptions_[req_id];
        vec.reserve(symbols.size());
        for (auto& s : symbols) {
            vec.push_back(intern_symbol(s));
        }
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Active subscriptions = " << active_symbols_.size()
            << " - Pending subscriptions = " << pending_subscriptions_.size());
    }

    // Register outbound unsubscribe request (called before sending an unsubscribe request)
    inline void register_unsubscription(std::vector<Symbol> symbols, ctrl::req_id_t req_id) noexcept {
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Registering unsubscription request (req_id=" << req_id << ")");
        auto& vec = pending_unsubscriptions_[req_id];
        vec.reserve(symbols.size());
        for (auto& s : symbols) {
            vec.push_back(intern_symbol(s));
        }
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Active subscriptions = " << active_symbols_.size()
            << " - Pending unsubscriptions = " << pending_unsubscriptions_.size());
    }

    // ------------------------------------------------------------
    // Process ACK messages
    // ------------------------------------------------------------

    inline void process_subscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Processing subscribe ACK for symbol {" << symbol << "} (req_id=" << req_id << ") - success=" << std::boolalpha << success);
        bool done = false;
        if (success) [[likely]] {
            done = confirm_subscription_(symbol, req_id);
            if (done) WK_INFO("[SUBMGR] <" << to_string(channel_) << "> Subscription ACCEPTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
        }
        else {
            done = reject_subscription_(symbol, req_id);
            if (done) WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Subscription REJECTED for symbol  {" << symbol << "} (req_id=" << req_id << ")");
        }
        if (!done) WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Subscription OMITTED for symbol {" << symbol << "} (unknown req_id=" << req_id << ")");
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Active subscriptions = " << active_symbols_.size() << " - Pending subscriptions = " << pending_subscriptions_.size());
    }

    inline void process_unsubscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Processing unsubscribe ACK for symbol {" << symbol << "} (req_id=" << req_id << ") - success=" << std::boolalpha << success);
        bool done = false;
        if (success) [[likely]] {
            done = confirm_unsubscription_(symbol, req_id);
            if (done) WK_INFO("[SUBMGR] <" << to_string(channel_) << "> Unsubscription ACCEPTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
        }
        else {
            done = reject_unsubscription_(symbol, req_id);
            if (done) WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unsubscription REJECTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
        }
        if (!done) WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unsubscription ACK omitted for symbol {" << symbol << "} (unknown req_id=" << req_id << ")");
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Active subscriptions = " << active_symbols_.size() << " - Pending unsubscriptions = " << pending_unsubscriptions_.size());
    }

    inline void try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        SymbolId symbol_id = intern_symbol(symbol);
        // Try to reject from pending subscriptions
        if (try_process_rejection_on_(pending_subscriptions_, req_id, symbol_id)) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Subscription REJECTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
            return;
        }
        // Try to reject from pending unsubscriptions
        if (try_process_rejection_on_(pending_unsubscriptions_, req_id, symbol_id)) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unsubscription REJECTED for symbol {" << symbol << "} (req_id=" << req_id << ")");
            return;
        }
    }

    // ------------------------------------------------------------
    // State queries
    // ------------------------------------------------------------

    // Returns true if there is at least one pending request not full ACKed yet
    [[nodiscard]]
    inline bool has_pending_requests() const noexcept {
        return !(pending_subscriptions_.empty() && pending_unsubscriptions_.empty());
    }

    // Number of pending subscriptions not full ACKed yet (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_subscription_requests() const noexcept {
        return pending_subscriptions_.size();
    }

    // Number of pending unsubscriptions not full ACKed yet (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_unsubscription_requests() const noexcept {
        return pending_unsubscriptions_.size();
    }

    // Number of pending protocol requests not full ACKed yet (grouped by req_id, not symbols)
    [[nodiscard]]
    inline std::size_t pending_requests() const noexcept {
        return pending_subscriptions_.size() + pending_unsubscriptions_.size();
    }

    // Number of pending subscribed symbols awaiting ACK (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_subscribe_symbols() const noexcept {
        std::size_t total = 0;
        for (const auto& [req_id, vec] : pending_subscriptions_) {
            total += vec.size();
        }
        return total;
    }

    // Number of pending unsubscribed symbols awaiting ACK (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_unsubscribe_symbols() const noexcept {
        std::size_t total = 0;
        for (const auto& [req_id, vec] : pending_unsubscriptions_) {
            total += vec.size();
        }
        return total;
    }

    // Number of pending symbols awaiting ACK (useful for debugging)
    [[nodiscard]]
    inline std::size_t pending_symbols() const noexcept {
        return pending_subscribe_symbols() + pending_unsubscribe_symbols();
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

    // ------------------------------------------------------------
    // Reset behavior
    // ------------------------------------------------------------

    // Full reset (e.g., on shutdown, or full reconnect)
    inline void clear_all() noexcept {
        pending_subscriptions_.clear();
        pending_unsubscriptions_.clear();
        active_symbols_.clear();
    }

private:
    Channel channel_;

    std::unordered_map<std::uint64_t, std::vector<SymbolId>> pending_subscriptions_;
    std::unordered_map<std::uint64_t, std::vector<SymbolId>> pending_unsubscriptions_;
    // Invariant:
    /// active_symbols_ contains exactly the set of symbols currently subscribed on the server for this channel.
    // It is mutated ONLY on successful subscribe/unsubscribe ACKs or reset.
    // Rejections never touch active_symbols_.
    std::unordered_set<SymbolId> active_symbols_;

private:
    // ------------------------------------------------------------
    // Subscribe handling
    // ------------------------------------------------------------

    // Process ACK for subscription. Called when we parse a successful {"method":"subscribe","success":true}
    [[nodiscard]] inline bool confirm_subscription_(const Symbol& symbol, ctrl::req_id_t req_id) noexcept {
        WK_DEBUG("[SUBMGR] <" << to_string(channel_) << "> Confirming subscription for req_id=" << req_id << " symbol {" << symbol << "}");
        SymbolId symbol_id = intern_symbol(symbol);
        // Find pending subscription by req_id
        auto it = pending_subscriptions_.find(req_id);
        if (it == pending_subscriptions_.end()) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unable to confirm subscription - unknown req_id=" << req_id);
            return false;
        }
        auto& vec = it->second;
        // erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unable to confirm subscription - symbol not found in pending (req_id=" << req_id << ")");
            return false;
        }
        // create active group now (if not exists)
        auto [active_it, inserted] = active_symbols_.insert(symbol_id);
        if (!inserted) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Symbol {" << symbol << "} already active");
        }
        // Erase symbol from pending list
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Subscription confirmed for symbol {" << symbol << "} (req_id=" << req_id << ")");
        vec.erase(pos);
        if (vec.empty()) { // If no symbols left → remove req_id entry
            WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> All pending subscriptions confirmed for req_id=" << req_id);
            pending_subscriptions_.erase(it);
        }

        return true;
    }

    // Process ACK for subscription. Called when a subscribe request returns success=false
    [[nodiscard]] inline bool reject_subscription_(const Symbol& symbol, ctrl::req_id_t req_id) noexcept {
        WK_DEBUG("[SUBMGR] <" << to_string(channel_) << "> Rejecting subscription for req_id=" << req_id << " symbol {" << symbol << "}");
        SymbolId symbol_id = intern_symbol(symbol);
        // Find pending subscription by req_id
        auto it = pending_subscriptions_.find(req_id);
        if (it == pending_subscriptions_.end()) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unable to reject subscription - no such pending request (req_id=" << req_id << ")");
            return false; // no such pending request
        }
        auto& vec = it->second;
        // erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unable to reject subscription - symbol not found in pending (req_id=" << req_id << ")");
            return false;
        }
        // Erase symbol from pending list
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Subscription rejected for symbol {" << symbol << "} (req_id=" << req_id << ")");
        vec.erase(pos);
        if (vec.empty()) { // If no symbols left → remove req_id entry
            WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> All pending subscriptions rejected for req_id=" << req_id);
            pending_subscriptions_.erase(it);
        }
        return true;
    }

    // ------------------------------------------------------------
    // Unsubscribe handling
    // ------------------------------------------------------------

    // Process ACK for unsubscription. Called when we parse a successful {"method":"unsubscribe","success":true}
    [[nodiscard]] inline bool confirm_unsubscription_(const Symbol& symbol, ctrl::req_id_t req_id) noexcept {
        WK_DEBUG("[SUBMGR] <" << to_string(channel_) << "> Confirming unsubscription for req_id=" << req_id << " symbol {" << symbol << "}");
        SymbolId symbol_id = intern_symbol(symbol);
        // Find pending subscription by req_id
        auto it = pending_unsubscriptions_.find(req_id);
        if (it == pending_unsubscriptions_.end()) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unable to confirm unsubscription - unknown req_id=" << req_id);
            return false;
        }
        auto& vec = it->second;
        // erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unable to confirm unsubscription - symbol not found in pending (req_id=" << req_id << ")");
            return false;
        }
        // Remove from active_symbols_ set
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Removing symbol {" << symbol << "} from active subscriptions (req_id=" << req_id << ")");
        std::size_t erased = active_symbols_.erase(symbol_id);
        if (erased == 0) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unsubscription ACK for non-active symbol {" << symbol << "}");
        }
        // Erase symbol from pending list
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Unsubscription confirmed for symbol {" << symbol << "} (req_id=" << req_id << ")");
        vec.erase(pos);
        if (vec.empty()) { // If no symbols left → remove req_id entry
            WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> All pending unsubscriptions confirmed for req_id=" << req_id);
            pending_unsubscriptions_.erase(it);
        }

        return true;
    }

    // Process ACK for unsubscription. Called when a subscribe request returns success=false
    [[nodiscard]] inline bool reject_unsubscription_(const Symbol& symbol, ctrl::req_id_t req_id) noexcept {
        WK_DEBUG("[SUBMGR] <" << to_string(channel_) << "> Rejecting unsubscription for req_id=" << req_id << " symbol {" << symbol << "}");
        SymbolId symbol_id = intern_symbol(symbol);
        // Find pending subscription by req_id
        auto it = pending_unsubscriptions_.find(req_id);
        if (it == pending_unsubscriptions_.end()) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unable to reject unsubscription - no such pending request (req_id=" << req_id << ")");
            return false; // no such pending request
        }
        auto& vec = it->second;
        // erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            WK_WARN("[SUBMGR] <" << to_string(channel_) << "> Unable to reject unsubscription - symbol not found in pending (req_id=" << req_id << ")");
            return false;
        }
        // Erase symbol from pending list
        WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> Unsubscription rejected for symbol {" << symbol << "} (req_id=" << req_id << ")");
        vec.erase(pos);
        if (vec.empty()) { // If no symbols left → remove req_id entry
            WK_TRACE("[SUBMGR] <" << to_string(channel_) << "> All pending unsubscriptions rejected for req_id=" << req_id);
            pending_unsubscriptions_.erase(it);
        }
        return true;
    }

    template <typename PendingMap>
    [[nodiscard]] inline bool try_process_rejection_on_(PendingMap& pending, ctrl::req_id_t req_id, SymbolId symbol_id) noexcept {
        // Find pending subscription by req_id
        auto it = pending.find(req_id);
        if (it == pending.end()) {
            return false;
        }
        // Erase symbol_id (O(n) but n<=10 always because Kraken allows up to 10 symbols per request)
        auto& vec = it->second;
        auto pos = std::find(vec.begin(), vec.end(), symbol_id);
        if (pos == vec.end()) {
            return false;
        }
        vec.erase(pos);
        // If no symbols left → remove req_id entry
        if (vec.empty()) {
            pending.erase(it);
        }

        return true;
    }

};

} // namespace channel
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
