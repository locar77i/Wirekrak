#pragma once

#include <cassert>
#include <string_view>
#include <vector>

#include "wirekrak/core/symbol.hpp"
#include "lcr/optional.hpp"
#include "lcr/trap.hpp"


namespace wirekrak::core::protocol::kraken::schema {

// ------------------------------------------------------------
// Validate common Kraken schema invariants (Debug only)
// ------------------------------------------------------------

inline void validate_symbols(const RequestSymbols& symbols) {
#ifndef NDEBUG
    LCR_ASSERT_MSG(!symbols.empty(), "Kraken request requires at least one symbol");

    for (const auto& s : symbols) {
        LCR_ASSERT_MSG(!std::string_view(s).empty(), "Kraken schema symbol cannot be empty");
    }
#else
    (void)symbols;
#endif
}

inline void validate_req_id(const lcr::optional<std::uint64_t>& req_id) {
#ifndef NDEBUG
    if (req_id.has()) {
        LCR_ASSERT_MSG(req_id.value() != 0, "Kraken schema req_id should be non-zero");
    }
#else
    (void)req_id;
#endif
}

} // namespace wirekrak::core::protocol::kraken::schema
