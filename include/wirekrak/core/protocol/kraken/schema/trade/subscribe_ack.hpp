#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace schema {
namespace trade {

struct SubscribeAck {
    bool success;
    Symbol symbol;

    lcr::optional<bool> snapshot;

    std::vector<std::string> warnings;
    lcr::optional<std::string> error;

    lcr::optional<Timestamp> time_in;
    lcr::optional<Timestamp> time_out;

    lcr::optional<ctrl::req_id_t> req_id;
};

} // namespace trade
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
