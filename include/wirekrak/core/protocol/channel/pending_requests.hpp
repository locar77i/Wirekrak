#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/protocol/control/req_id.hpp"
#include "lcr/trap.hpp"


namespace wirekrak::core::protocol::channel {

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
    inline void add(ctrl::req_id_t req_id, const RequestSymbolIds& sids) noexcept {
        auto& vec = requests_[req_id];

        for (const auto& sid : sids) {
            // Enforce uniqueness at pending layer
            if (pending_symbols_.contains(sid)) {
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
    [[nodiscard]]
    inline bool remove(ctrl::req_id_t req_id, SymbolId sid) noexcept {

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
    [[nodiscard]]
    inline bool remove(SymbolId sid) noexcept {

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

        // Should never happen if invariants hold
        LCR_UNREACHABLE();

        return false;
    }

    // ------------------------------------------------------------
    // Queries
    // ------------------------------------------------------------

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

        LCR_ASSERT_MSG(count == pending_symbols_.size(), "pending_symbols_ size must match total symbols in requests_");
    }
#endif

private:
    std::unordered_map<ctrl::req_id_t, std::vector<SymbolId>> requests_;
    std::unordered_set<SymbolId> pending_symbols_;
};

} // namespace wirekrak::core::protocol::channel
