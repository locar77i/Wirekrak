#pragma once

#include "wirekrak/core/transport/websocket/engine.hpp"
#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/policy/transport/websocket_bundle.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"
#include "wirekrak/core/preset/transport/backend_default.hpp"


namespace wirekrak::core::preset::transport {

    using DefaultWebSocket =
        core::transport::websocket::Engine<
            DefaultControlRing,
            DefaultMessageRing,
            policy::transport::DefaultWebsocket,
            preset::transport::DefaultBackend
        >;

    // Assert that DefaultWebSocket conforms to transport::WebSocketConcept concept
    static_assert(core::transport::WebSocketConcept<DefaultWebSocket>);

} // namespace wirekrak::core::preset::transport
