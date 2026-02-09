#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <algorithm>

#include "wirekrak/core/symbol/intern.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"

namespace wirekrak::lite::channel {

/*
================================================================================
 Dispatcher<MessageT>  (Hybrid, Hot-Path Optimized)
================================================================================

A channel-specific dispatcher designed for **high-frequency data-plane routing**
with **deterministic lifecycle management**.

--------------------------------------------------------------------------------
 Architectural role
--------------------------------------------------------------------------------

• Executes user code
• Routes messages to callbacks
• Owns callback lifetime
• Responds to protocol rejections via req_id

Core produces facts. Lite turns facts into behavior.

--------------------------------------------------------------------------------
 Design goals
--------------------------------------------------------------------------------

1. **Fast hot path**
   Dispatch must be as close as possible to:
       symbol → callbacks → execute

2. **Authoritative req_id ownership**
   All lifecycle events (reject, unsubscribe, replay) are expressed in terms of
   req_id, not symbols.

3. **Deterministic cleanup**
   Removing a rejected request must:
     • remove all associated callbacks
     • touch only data related to that req_id
     • never scan unrelated subscriptions

--------------------------------------------------------------------------------
 Key insight
--------------------------------------------------------------------------------

Hot path and cold path have different requirements:

• Hot path (dispatch):
    - extremely frequent
    - must be flat and cache-friendly

• Cold path (rejection / unsubscribe):
    - rare
    - may do bounded work

This dispatcher explicitly optimizes **both**, instead of compromising one.

================================================================================
*/

template<class MessageT>
class Dispatcher {
public:
    using Callback = std::function<void(const MessageT&)>;

    // -------------------------------------------------------------------------
    // Registration
    // -------------------------------------------------------------------------

    /*
        Register a subscription.

        Parameters:
          • req_id   - authoritative identity assigned by Core
          • symbols  - symbols included in the subscription request
          • cb       - user callback

        Semantics:
          • One req_id corresponds to one callback
          • That callback may be associated with N symbols
          • The callback will be invoked once per matching message
    */
    inline void add(
        wirekrak::core::protocol::ctrl::req_id_t req_id,
        const std::vector<wirekrak::core::Symbol>& symbols,
        Callback cb)
    {
        // Record all symbols associated with this req_id
        std::vector<wirekrak::core::SymbolId> interned_symbols;
        interned_symbols.reserve(symbols.size());

        for (const auto& s : symbols) {
            wirekrak::core::SymbolId sid = wirekrak::core::intern_symbol(s);
            interned_symbols.push_back(sid);

            // HOT-PATH STRUCTURE:
            // symbol → vector of (req_id, callback)
            by_symbol_[sid].push_back(Entry{
                .req_id   = req_id,
                .callback = cb
            });
        }

        // COLD-PATH STRUCTURE:
        // req_id → list of symbols (for deterministic removal)
        by_req_id_.emplace(req_id, std::move(interned_symbols));
    }

    // -------------------------------------------------------------------------
    // Dispatch (HOT PATH)
    // -------------------------------------------------------------------------

    /*
        Dispatch a message to all callbacks registered for its symbol.

        HOT PATH properties:
          • Single hash lookup
          • Linear scan over a tight vector
          • No secondary maps
          • No dynamic allocation
          • No protocol logic

        This is intentionally as flat as possible.
    */
    inline void dispatch(const MessageT& msg) const {
        wirekrak::core::SymbolId sid = wirekrak::core::intern_symbol(msg.get_symbol());

        auto it = by_symbol_.find(sid);
        if (it == by_symbol_.end())
            return;

        // Tight loop: execute callbacks directly
        for (const Entry& e : it->second) {
            e.callback(msg);
        }
    }

    // -------------------------------------------------------------------------
    // Removal by req_id (COLD PATH)
    // -------------------------------------------------------------------------

    /*
        Remove all callbacks associated with a req_id.

        Invoked when:
          • A subscription is rejected
          • An unsubscribe ACK is received
          • Lite explicitly cancels behavior

        Complexity:
          • O(number_of_symbols_in_request)
          • No scanning of unrelated subscriptions

        This is cold-path code and intentionally prioritizes correctness and
        determinism over micro-optimizations.
    */
    inline void remove_by_req_id(wirekrak::core::protocol::ctrl::req_id_t req_id)
    {
        auto it = by_req_id_.find(req_id);
        if (it == by_req_id_.end())
            return;

        // For each symbol associated with this req_id,
        // remove the corresponding callback entries.
        for (wirekrak::core::SymbolId sid : it->second) {
            auto sym_it = by_symbol_.find(sid);
            if (sym_it == by_symbol_.end())
                continue;

            auto& vec = sym_it->second;

            vec.erase(
                std::remove_if(vec.begin(), vec.end(),
                    [&](const Entry& e) {
                        return e.req_id == req_id;
                    }),
                vec.end()
            );

            // Clean up empty symbol buckets
            if (vec.empty()) {
                by_symbol_.erase(sym_it);
            }
        }

        // Finally remove the req_id ownership record
        by_req_id_.erase(it);
    }

    // -------------------------------------------------------------------------
    // Quiescence
    // -------------------------------------------------------------------------

    /*
        Returns true if the dispatcher is idle.

        Invariant: by_req_id_.empty() -> by_symbol_.empty()

        Dispatcher-idle means:
        • No callbacks are registered
        • No req_id ownership remains
        • dispatch() would execute no user code

        This is a behavioral quiescence signal only.
        It does NOT imply anything about:
        • protocol state
        • active subscriptions on the exchange
        • future messages

        Intended use:
        • graceful shutdown
        • drain loops
        • Lite client idleness checks

        Complexity:
        • O(1)
    */
    [[nodiscard]]
    inline bool is_idle() const noexcept {
        return by_req_id_.empty();
    }

    // -------------------------------------------------------------------------
    // Full reset
    // -------------------------------------------------------------------------

    /*
        Clear all routing state.

        Used on:
          • reconnect
          • shutdown
          • Lite session reset

        Core replay will re-establish protocol intent as needed.
    */
    inline void clear() noexcept {
        by_symbol_.clear();
        by_req_id_.clear();
    }

private:
    /*
        Entry stored in the HOT-PATH structure.

        This is intentionally small and cache-friendly:
          • req_id   - used only for cold-path removal
          • callback - executed directly in dispatch
    */
    struct Entry {
        wirekrak::core::protocol::ctrl::req_id_t req_id;
        Callback callback;
    };

    // HOT PATH:
    //   symbol → callbacks
    std::unordered_map<wirekrak::core::SymbolId, std::vector<Entry>> by_symbol_;

    // COLD PATH:
    //   req_id → symbols
    std::unordered_map<wirekrak::core::protocol::ctrl::req_id_t, std::vector<wirekrak::core::SymbolId>> by_req_id_;
};

} // namespace wirekrak::lite::channel
