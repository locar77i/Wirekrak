#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"

#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"


namespace wirekrak::core::preset::transport {

    using DefaultWebSocket =
        wirekrak::core::transport::winhttp::WebSocketImpl<
            DefaultControlRing,
            DefaultMessageRing
        >;

    // Assert that DefaultWebSocket conforms to transport::WebSocketConcept concept
    static_assert(wirekrak::core::transport::WebSocketConcept<DefaultWebSocket>);

} // namespace wirekrak::core::preset::transport
