#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace rejection {

// ===============================================
// REJECTION NOTICE
// Represents a failed request acknowledgement
//
// Failed Kraken acknowledgements are normalized into a single
// rejection::Notice type for consistent error handling.
// ===============================================
struct Notice {
    std::string error;
    lcr::optional<std::uint64_t> req_id;
    lcr::optional<Symbol> symbol;
    lcr::optional<Timestamp> time_in;
    lcr::optional<Timestamp> time_out;
};

} // namespace rejection
} // namespace kraken
} // namespace protocol
} // namespace wirekrak