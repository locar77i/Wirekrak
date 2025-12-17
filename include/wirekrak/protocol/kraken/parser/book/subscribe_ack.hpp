#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/parser/detail/book_ack_common.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct subscribe_ack {
    static bool parse(const simdjson::dom::element& root, kraken::book::SubscribeAck& out) noexcept {
        if (!detail::parse_book_ack_common(root, "subscribe", out))
            return false;

        // subscribe-only fields
        auto snapshot = root["result"]["snapshot"].get_bool();
        if (snapshot.error())
            return false;
        out.snapshot = snapshot.value();

        auto warnings = root["result"]["warnings"];
        if (!warnings.error()) {
            for (auto w : warnings.get_array())
                out.warnings.emplace_back(std::string(w.get_string().value()));
        }

        return true;
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
