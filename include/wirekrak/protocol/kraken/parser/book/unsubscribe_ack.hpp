#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/schema/book/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/parser/book/detail/parse_ack_common.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct unsubscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, schema::book::UnsubscribeAck& out) noexcept {
        out = schema::book::UnsubscribeAck{};
        return detail::parse_ack_common(root, "unsubscribe", out);
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
