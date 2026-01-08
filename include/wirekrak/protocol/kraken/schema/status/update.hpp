#pragma once

#include <string>
#include <cstdint>
#include <ostream>
#include <sstream>

#include "wirekrak/protocol/kraken/enums/system_state.hpp"
#include "lcr/optional.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace schema {
namespace status {

/*
===============================================================================
Kraken System Status Update
===============================================================================

Represents a "status" channel update message sent by Kraken WebSocket API v2.

Example payload:
{
  "channel": "status",
  "type": "update",
  "data": [{
      "system": "online",
      "api_version": "v2",
      "connection_id": 123456789,
      "version": "1.9.0"
  }]
}

The status object is always the first and only element in `data`.
===============================================================================
*/


struct Update {
    SystemState system;             // Trading engine state
    std::string api_version;        // WebSocket API version (e.g. "v2")
    std::uint64_t connection_id;    // Unique connection identifier
    std::string version;            // WebSocket service version

    // ------------------------------------------------------------
    // Debug / diagnostic dump
    // ------------------------------------------------------------
    inline void dump(std::ostream& os) const noexcept {
        os << "[STATUS] { "
           << "system=" << to_string(system) << ", "
           << "api_version=" << api_version << ", "
           << "connection_id=" << connection_id << ", "
           << "version=" << version
           << " }";
    }

    // ------------------------------------------------------------
    // String helper
    // ------------------------------------------------------------
    [[nodiscard]]
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
};

// Stream operator
inline std::ostream& operator<<(std::ostream& os, const Update& u) {
    u.dump(os);
    return os;
}

} // namespace status
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
