/*
===============================================================================
Replay Subscription<RequestT> (Protocol Intent Unit)
===============================================================================

A Subscription represents **one acknowledged protocol request** together with
its remaining active symbols.

It is the smallest unit of replayable intent in the Kraken Session and is owned
exclusively by a replay::Table<RequestT>.

-------------------------------------------------------------------------------
Role in the system
-------------------------------------------------------------------------------
• Encapsulates a single outbound protocol request (`RequestT`)
• Owns the request's `req_id` and symbol set
• Supports symbol-level mutation due to:
    - explicit unsubscribe
    - protocol rejection
• Determines when a request becomes empty and must be discarded

-------------------------------------------------------------------------------
Key semantics
-------------------------------------------------------------------------------
• One Subscription == one req_id
• One Subscription may contain N symbols
• Symbols are removed individually
• When no symbols remain, the subscription is considered dead
• Dead subscriptions are removed eagerly by the owning Table

-------------------------------------------------------------------------------
Protocol correctness rules
-------------------------------------------------------------------------------
• A rejected symbol is removed permanently
• A rejected subscription is never replayed
• A subscription with zero symbols MUST NOT be replayed
• No inference or repair is performed

-------------------------------------------------------------------------------
What this class deliberately does NOT do
-------------------------------------------------------------------------------
• Does NOT store callbacks or user behavior
• Does NOT dispatch messages
• Does NOT replay itself
• Does NOT perform I/O
• Does NOT infer protocol state

-------------------------------------------------------------------------------
Threading & lifetime
-------------------------------------------------------------------------------
• Not thread-safe
• Mutated only by the Session event loop
• Lives inside a replay::Table
• Moved, never copied

-------------------------------------------------------------------------------
Usage
-------------------------------------------------------------------------------
• erase_symbol(symbol)
    Removes a symbol from the request

• try_process_rejection(req_id, symbol)
    Applies a protocol rejection if req_id matches

• empty()
    Indicates that the subscription should be discarded

===============================================================================
*/

#pragma once

#include <cstdint>
#include <utility>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"

namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace replay {

// ------------------------------------------------------------
// Per-entry object: stores request + symbol ops
// ------------------------------------------------------------
template<class RequestT>
class Subscription {
public:
    Subscription(RequestT req)
        : request_(std::move(req))
    {}

    [[nodiscard]]
    inline bool erase_symbol(Symbol symbol) {
        auto& symbols = request_.symbols;
        auto it = std::remove_if(symbols.begin(), symbols.end(),[&](const Symbol& sym) { return sym == symbol; });
        bool erased = (it != symbols.end());
        if (erased) {
            symbols.erase(it, symbols.end());
            WK_TRACE("[REPLAY:SUBSCRIPTION] Erased symbol {" << symbol << "}" << " from subscription (req_id=" << req_id() << ")");
        }
        return erased;
    }

    [[nodiscard]]
    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        bool done = false;
        if (request_.req_id.has() && request_.req_id.value() == req_id) {
            // Match found → erase symbol
            done = erase_symbol(symbol);
        }
        return done;
    }

    [[nodiscard]]
    inline bool empty() const noexcept {
        return request_.symbols.empty();
    }

    [[nodiscard]] inline const RequestT& request() const noexcept { return request_; }
    [[nodiscard]] inline RequestT& request() noexcept { return request_; }

    [[nodiscard]] inline ctrl::req_id_t req_id() const noexcept {
        return request_.req_id.has() ? request_.req_id.value() : 0;
    }

private:
    RequestT request_;
};

} // namespace replay
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
