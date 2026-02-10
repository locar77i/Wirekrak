#pragma once

#include <unordered_map>
#include <vector>
#include <functional>

#include "wirekrak/core/symbol/intern.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::lite::channel {

/*
================================================================================
 Dispatcher<MessageT>  (Lite, Symbol-Authoritative)
================================================================================

A channel-specific dispatcher designed for **high-frequency data-plane routing**
with **symbol-scoped lifecycle management**.

--------------------------------------------------------------------------------
 Architectural role
--------------------------------------------------------------------------------

• Executes user code
• Routes messages to callbacks
• Owns callback lifetime
• Responds to protocol events via symbol identity

Core produces facts. Lite turns facts into behavior.

--------------------------------------------------------------------------------
 Design goals
--------------------------------------------------------------------------------

1. **Fast hot path**
   Dispatch must be as close as possible to:
       symbol → callbacks → execute

2. **Symbol-authoritative lifecycle**
   Lite manages behavior strictly in terms of symbols.
   Protocol identifiers (req_id) never escape Core.

3. **Deterministic cleanup**
   Removing a symbol must:
     • remove all associated callbacks
     • touch only data related to that symbol
     • never scan unrelated subscriptions

--------------------------------------------------------------------------------
 Key insight
--------------------------------------------------------------------------------

Lite does NOT model protocol intent.

• Core tracks protocol correctness
• Lite tracks user-visible behavior
• Symbols are the only stable, user-level identity

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
          • symbols  - symbols included in the subscription request
          • cb       - user callback

        Semantics:
          • That callback may be associated with N symbols
          • The callback will be invoked once per matching message
    */
    inline void add(const std::vector<wirekrak::core::Symbol>& symbols, Callback cb) {
        WK_TRACE("[DISPATCHER] Adding callbacks for " << symbols.size() << " symbol(s)");

        for (const auto& s : symbols) {
            wirekrak::core::SymbolId sid = wirekrak::core::intern_symbol(s);

            // HOT-PATH STRUCTURE:
            // symbol → vector of (callback)
            symbols_map_[sid].push_back(cb);
        }
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

        auto it = symbols_map_.find(sid);
        if (it == symbols_map_.end())
            return;

        // Tight loop: execute callbacks directly
        for (const Callback& cb : it->second) {
            cb(msg);
        }
    }


    // -------------------------------------------------------------------------
    // Removal by symbol (COLD PATH, Lite policy)
    // -------------------------------------------------------------------------

    inline void remove(wirekrak::core::Symbol symbol) {
        WK_TRACE("[DISPATCHER] Removing callbacks by symbol (symbol=" << symbol << ")");

        wirekrak::core::SymbolId sid = wirekrak::core::intern_symbol(symbol);

        auto it = symbols_map_.find(sid);
        if (it == symbols_map_.end())
            return;

        // Finally remove the symbol bucket
        symbols_map_.erase(it);
    }

    inline void remove(const std::vector<wirekrak::core::Symbol>& symbols) {
        WK_TRACE("[DISPATCHER] Removing callbacks for " << symbols.size() << " symbol(s)");

        for (const auto& symbol : symbols) {
            wirekrak::core::SymbolId sid = wirekrak::core::intern_symbol(symbol);
            symbols_map_.erase(sid);
        }
    }

    // -------------------------------------------------------------------------
    // Quiescence
    // -------------------------------------------------------------------------

    /*
        Returns true if the dispatcher is idle.

        Dispatcher-idle means:
        • No callbacks are registered
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
        return symbols_map_.empty();
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
        symbols_map_.clear();
    }

private:
    // HOT PATH:
    //   symbol → callbacks
    std::unordered_map<wirekrak::core::SymbolId, std::vector<Callback>> symbols_map_;
};

} // namespace wirekrak::lite::channel
