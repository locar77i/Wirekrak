#pragma once

#include <cassert>
#include <string_view>
#include <vector>

#include "wirekrak/core/symbol.hpp"
#include "lcr/optional.hpp"

namespace wirekrak::core::protocol::kraken::request {

// ------------------------------------------------------------
// Validate common Kraken request invariants (Debug only)
// ------------------------------------------------------------

inline void validate_symbols(const std::vector<Symbol>& symbols) {
#ifndef NDEBUG
    assert(!symbols.empty() && "Kraken request requires at least one symbol");

    for (const auto& s : symbols) {
        assert(!std::string_view(s).empty() && "Kraken request symbol cannot be empty");
    }
#else
    (void)symbols;
#endif
}

inline void validate_req_id(const lcr::optional<std::uint64_t>& req_id) {
#ifndef NDEBUG
    if (req_id.has()) {
        assert(req_id.value() != 0 && "Kraken request req_id should be non-zero");
    }
#else
    (void)req_id;
#endif
}

} // namespace wirekrak::core::protocol::kraken::request
