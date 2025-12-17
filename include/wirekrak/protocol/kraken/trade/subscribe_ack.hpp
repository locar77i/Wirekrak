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
namespace trade {

struct SubscribeAck {
    bool success = false;
    Symbol symbol;
    lcr::optional<std::uint64_t> req_id;
    lcr::optional<bool> snapshot;

    lcr::optional<std::vector<std::string>> warnings;
    lcr::optional<std::string> error;

    lcr::optional<Timestamp> time_in;
    lcr::optional<Timestamp> time_out;
};

} // namespace trade
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
