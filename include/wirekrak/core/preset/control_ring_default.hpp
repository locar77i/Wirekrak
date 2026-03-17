#pragma once

#include "wirekrak/core/transport/websocket/events.hpp"
#include "wirekrak/core/config/transport/websocket.hpp"
#include "lcr/lockfree/spsc_queue.hpp"


namespace wirekrak::core::preset {

    using DefaultControlRing =
        lcr::lockfree::spsc_queue<
        transport::websocket::Event,
        config::transport::CONTROL_RING_CAPACITY
    >;

} // namespace wirekrak::core::preset
