#pragma once

#include "wirekrak/core/protocol/session.hpp"
#include "wirekrak/core/protocol/kraken_model.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"
#include "wirekrak/core/preset/transport/websocket_default.hpp"


namespace wirekrak::core::preset::protocol::kraken {

    using DefaultSession =
        wirekrak::core::protocol::Session<
            wirekrak::core::protocol::KrakenModel,
            transport::DefaultWebSocket,
            DefaultMessageRing
        >;

} // namespace wirekrak::core::preset::protocol::kraken
