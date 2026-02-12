#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/symbol/intern.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core::protocol::kraken::channel {

/*
===============================================================================
PendingRequests
===============================================================================

Purpose
-------
Tracks pending protocol requests at symbol granularity.

Each request:
  req_id -> vector<SymbolId>

Additionally:
  pending_symbols_ ensures symbol-level uniqueness and O(1) lookup.

Core Invariants
---------------
• A SymbolId may appear at most once across all pending requests.
• pending_symbols_.size() equals total number of stored symbols.
• If a request vector becomes empty, it is erased.
• Not thread-safe (event-loop only).

Design
------
• Vector is used per request (Kraken ≤ 10 symbols per request).
• Symbol uniqueness enforced globally inside this container.
• Policy-neutral: does not decide whether duplicates are valid;
  simply prevents duplication inside pending state.

===============================================================================
*/

class PendingRequests {
public:
    PendingRequests() = default;

    // ------------------------------------------------------------
    // Add a new pending request
    // ------------------------------------------------------------
    inline void add(ctrl::req_id_t req_id, const std::vector<Symbol>& symbols) noexcept {
        auto& vec = requests_[req_id];

        for (const auto& symbol : symbols) {
            SymbolId sid = intern_symbol(symbol);

            // Enforce uniqueness at pending layer
            if (pending_symbols_.contains(sid)) {
                WK_TRACE("[PENDING] Ignoring duplicate pending symbol {" 
                         << symbol << "} (req_id=" << req_id << ")");
                continue;
            }

            vec.push_back(sid);
            pending_symbols_.insert(sid);
        }

        if (vec.empty()) {
            // If nothing was inserted, remove empty req_id entry
            requests_.erase(req_id);
        }
    }

    // ------------------------------------------------------------
    // Remove a specific symbol from a specific request
    // Returns true if removed
    // ------------------------------------------------------------
    inline bool remove(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        SymbolId sid = intern_symbol(symbol);

        auto it = requests_.find(req_id);
        if (it == requests_.end())
            return false;

        auto& vec = it->second;

        auto pos = std::find(vec.begin(), vec.end(), sid);
        if (pos == vec.end())
            return false;

        vec.erase(pos);
        pending_symbols_.erase(sid);

        if (vec.empty()) {
            requests_.erase(it);
        }

        return true;
    }

    // ------------------------------------------------------------
    // Remove a symbol globally (owner lookup)
    // Returns true if removed
    // ------------------------------------------------------------
    inline bool remove_symbol(Symbol symbol) noexcept {
        SymbolId sid = intern_symbol(symbol);

        if (!pending_symbols_.contains(sid))
            return false;

        for (auto it = requests_.begin(); it != requests_.end(); ++it) {
            auto& vec = it->second;
            auto pos = std::find(vec.begin(), vec.end(), sid);
            if (pos != vec.end()) {
                vec.erase(pos);
                pending_symbols_.erase(sid);

                if (vec.empty()) {
                    requests_.erase(it);
                }

                return true;
            }
        }

#ifndef NDEBUG
        // Should never happen if invariants hold
        assert(false && "pending_symbols_ inconsistent with requests_");
#endif

        return false;
    }

    // ------------------------------------------------------------
    // Queries
    // ------------------------------------------------------------

    [[nodiscard]]
    inline bool contains(Symbol symbol) const noexcept {
        return pending_symbols_.contains(intern_symbol(symbol));
    }

    [[nodiscard]]
    inline bool contains(SymbolId sid) const noexcept {
        return pending_symbols_.contains(sid);
    }

    [[nodiscard]]
    inline bool empty() const noexcept {
        return requests_.empty();
    }

    [[nodiscard]]
    inline std::size_t count() const noexcept {
        return requests_.size();
    }

    [[nodiscard]]
    inline std::size_t symbol_count() const noexcept {
        return pending_symbols_.size();
    }

    // ------------------------------------------------------------
    // Clear
    // ------------------------------------------------------------
    inline void clear() noexcept {
        requests_.clear();
        pending_symbols_.clear();
    }

#ifndef NDEBUG
    // ------------------------------------------------------------
    // Invariant checker (debug only)
    // ------------------------------------------------------------
    inline void assert_consistency() const  {
        std::size_t count = 0;
        for (const auto& [_, vec] : requests_) {
            count += vec.size();
        }

        assert(count == pending_symbols_.size());
    }
#endif

private:
    std::unordered_map<ctrl::req_id_t, std::vector<SymbolId>> requests_;
    std::unordered_set<SymbolId> pending_symbols_;
};

} // namespace wirekrak::core::protocol::kraken::channel
