#pragma once

/*
===============================================================================
PendingRequests (Symbol-Level Pending Index)
===============================================================================

Purpose
-------
Tracks pending protocol requests at symbol granularity.

This structure provides a bidirectional mapping:

    req_id -> vector<SymbolId>
    SymbolId -> (implicitly owned by exactly one req_id)

and enforces **global uniqueness of SymbolId** across all pending requests.

-------------------------------------------------------------------------------
Core Invariants
-------------------------------------------------------------------------------

• A SymbolId may appear at most once across all pending requests
• pending_symbols_.size() equals total number of stored symbols
• If a request vector becomes empty, it is erased
• All operations are O(1) average (unordered containers + small vectors)
• Not thread-safe (event-loop only)

-------------------------------------------------------------------------------
Semantics
-------------------------------------------------------------------------------

• add():
    - Inserts symbols under a req_id
    - Silently ignores duplicates already present globally

• remove(req_id, sid):
    - Removes a specific symbol from a specific request

• remove(sid):
    - Removes a symbol globally (owner lookup required)

• contains():
    - Fast membership test for pending symbols

-------------------------------------------------------------------------------
Design
-------------------------------------------------------------------------------

• Vector per request:
    - Optimized for small batches (e.g. Kraken ≤ 10 symbols/request)

• Global uniqueness:
    - Enforced via pending_symbols_ (O(1) lookup)

• No ordering guarantees:
    - Requests and symbols are treated as sets for correctness

-------------------------------------------------------------------------------
System Role
-------------------------------------------------------------------------------

• Pure data structure (no protocol semantics)
• No timing, no progress tracking, no lifecycle decisions
• Used internally by:

    → subscription::Manager (symbol-level state machine)

Higher-level components define meaning:

    → Controller : progress + timeout policy
    → Session    : quiescence and shutdown behavior

-------------------------------------------------------------------------------
Notes
-------------------------------------------------------------------------------

• Duplicate suppression is structural, not policy-driven
• Consistency between requests_ and pending_symbols_ is critical
• Debug builds can validate invariants via assert_consistency()

===============================================================================
*/

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/protocol/control/req_id.hpp"
#include "lcr/trap.hpp"


namespace wirekrak::core::protocol::subscription {

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

} // namespace wirekrak::core::protocol::subscription
