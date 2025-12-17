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
// BOOK SUBSCRIBE ACK RESPONSE
// ===============================================
struct SubscribeAck {
    Symbol symbol;
    std::uint32_t depth;
    bool snapshot;
    bool success;

    lcr::optional<std::string> error;
    std::vector<std::string> warnings;

    lcr::optional<Timestamp> time_in;
    lcr::optional<Timestamp> time_out;

    lcr::optional<std::uint64_t> req_id;
};

} // namespace book
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
