#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/parser/book/detail/parse_ack_common.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct subscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::book::SubscribeAck& out) noexcept {
        return detail::parse_ack_common(root, "subscribe", out);
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
