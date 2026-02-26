#include "wirekrak/core/transport/connection.hpp"

#include "wirekrak/core/preset/message_ring_default.hpp"
#include "wirekrak/core/preset/transport/websocket_default.hpp"


namespace wirekrak::core::preset::transport {
  
    using DefaultConnection =
        wirekrak::core::transport::Connection<
            DefaultWebSocket,
            DefaultMessageRing
        >;

} // namespace wirekrak::core::preset::transport
