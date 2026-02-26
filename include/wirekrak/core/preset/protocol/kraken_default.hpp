#include "wirekrak/core/protocol/kraken/session.hpp"

#include "wirekrak/core/preset/message_ring_default.hpp"
#include "wirekrak/core/preset/transport/websocket_default.hpp"

namespace wirekrak::core::preset::protocol::kraken {
    
    using DefaultSession =
        wirekrak::core::protocol::kraken::Session<
            transport::DefaultWebSocket,
            DefaultMessageRing
        >;

} // namespace wirekrak::core::preset::protocol::kraken
