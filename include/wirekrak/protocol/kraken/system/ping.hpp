#pragma once

#include <cstdint>
#include <string>

#include "wirekrak/protocol/kraken/request/validate.hpp"
#include "lcr/json.hpp"
#include "lcr/optional.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace system {

struct Ping {
    lcr::optional<std::uint64_t> req_id{};

    std::string to_json() const {
#ifndef NDEBUG
        request::validate_req_id(req_id);
#endif
        std::string j;
        j.reserve(64);

        j += "{\"method\":\"ping\"";

        // --- req_id ---
        if (req_id.has()) {
            j += ",\"req_id\":";
            lcr::json::append(j, req_id.value());   // fast numeric append
        }

        j += "}";

        return j;
    }
};

} // namespace system
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
