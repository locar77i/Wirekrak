#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/schema/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/parser/book/detail/parse_ack_common.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct subscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, schema::book::SubscribeAck& out) noexcept {
        out = schema::book::SubscribeAck{};
        return detail::parse_ack_common(root, "subscribe", out);
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
