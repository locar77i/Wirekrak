#pragma once

#include <simdjson.h>

#include "wirekrak/core/protocol/kraken/schema/status/update.hpp"
#include "wirekrak/core/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/core/protocol/kraken/parser/adapters.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace parser {
namespace status {

class update {
public:
    // Parse a Kraken "status" channel update
    //
    // Expected shape:
    // {
    //   "channel": "status",
    //   "type": "update",
    //   "data": [ { ... } ]
    // }
    [[nodiscard]]
    static inline Result parse(const simdjson::dom::element& root, schema::status::Update& out) noexcept
    {
        using namespace simdjson;

        // Root must be an object
        auto r = helper::require_object(root);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Root not an object in status update -> ignore message.");
            return r;
        }

        // data must be an array
        dom::array data;
        r = helper::parse_array_required(root, "data", data);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'data' missing or invalid in status update -> ignore message.");
            return r;
        }

        // Kraken guarantees exactly one object
        auto it = data.begin();
        if (it == data.end()) {
            WK_DEBUG("[PARSER] Empty 'data' array in status update -> ignore message.");
            return Result::InvalidSchema;
        }

        const dom::element& obj = *it;

        // system (required)
        r = adapter::parse_system_state_required(obj, "system", out.system);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'system' invalid or missing in status update -> ignore message.");
            return r;
        }

        // api_version (required)
        std::string_view api_sv;
        r = helper::parse_string_required(obj, "api_version", api_sv);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'api_version' missing in status update -> ignore message.");
            return r;
        }
        out.api_version.assign(api_sv);

        // connection_id (required)
        r = helper::parse_uint64_required(obj, "connection_id", out.connection_id);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'connection_id' missing or invalid in status update -> ignore message.");
            return r;
        }

        // version (required)
        std::string_view sv;
        r = helper::parse_string_required(obj, "version", sv);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'version' missing in status update -> ignore message.");
            return r;
        }
        out.version.assign(sv);

        return Result::Parsed;
    }
};

} // namespace status
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
