#pragma once

#include <string>
#include <cstdint>

#include "wirekrak/protocol/kraken/enums/system_state.hpp"
#include "lcr/optional.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
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
};

} // namespace status
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
