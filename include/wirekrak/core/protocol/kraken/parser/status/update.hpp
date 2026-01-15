#pragma once

#include <simdjson.h>

#include "wirekrak/core/protocol/kraken/schema/status/update.hpp"
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
    static inline bool parse(const simdjson::dom::element& root, schema::status::Update& out) noexcept
    {
        using namespace simdjson;

        // data must be an array
        dom::array data;
        if (root["data"].get(data)) {
            WK_DEBUG("[PARSER] Field 'data' missing or invalid in status update -> ignore message.");
            return false;
        }

        // Kraken guarantees exactly one object
        auto it = data.begin();
        if (it == data.end()) {
            WK_DEBUG("[PARSER] Empty 'data' array in status update -> ignore message.");
            return false;
        }

        const dom::element& obj = *it;

        // system (required)
        std::string_view system_sv;
        if (obj["system"].get(system_sv)) {
            WK_DEBUG("[PARSER] Field 'system' missing in status update -> ignore message.");
            return false;
        }
        out.system = to_system_state_enum_fast(system_sv);
        if (out.system == SystemState::Unknown) {
            WK_DEBUG("[PARSER] Unknown system state '" << system_sv << "' -> ignore message.");
            return false;
        }

        // api_version (required)
        std::string_view api_sv;
        if (obj["api_version"].get(api_sv)) {
            WK_DEBUG("[PARSER] Field 'api_version' missing in status update -> ignore message.");
            return false;
        }
        out.api_version.assign(api_sv);

        // connection_id (required)
        if (obj["connection_id"].get(out.connection_id)) {
            WK_DEBUG("[PARSER] Field 'connection_id' missing in status update -> ignore message.");
            return false;
        }

        // version (required)
        std::string_view version_sv;
        if (obj["version"].get(version_sv)) {
            WK_DEBUG("[PARSER] Field 'version' missing in status update -> ignore message.");
            return false;
        }
        out.version.assign(version_sv);

        return true;
    }
};

} // namespace status
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
