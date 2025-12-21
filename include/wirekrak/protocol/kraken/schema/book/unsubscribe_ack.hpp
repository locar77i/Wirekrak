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
namespace book {

// ===============================================
// BOOK UNSUBSCRIBE ACK RESPONSE
// ===============================================
//
// Acknowledgement message returned by Kraken after
// an unsubscribe request to the book channel.
//
struct UnsubscribeAck {
    // Required fields
    Symbol symbol;
    std::uint32_t depth;
    bool success;

    // Conditional error (present if success == false)
    lcr::optional<std::string> error;

    // Optional timestamps
    lcr::optional<Timestamp> time_in;
    lcr::optional<Timestamp> time_out;

    // Optional client request id
    lcr::optional<std::uint64_t> req_id;
};

} // namespace book
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
