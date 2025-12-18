#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace trade {

struct subscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::trade::SubscribeAck& out) noexcept {
        return false; // TODO
    }
};

} // namespace trade
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
