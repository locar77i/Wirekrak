#include "wirekrak/core/config/transport/websocket.hpp"
#include "lcr/lockfree/spsc_ring.hpp"


namespace wirekrak::core::preset {

    using DefaultControlRing =
        lcr::lockfree::spsc_ring<
        wirekrak::core::transport::websocket::Event,
        wirekrak::core::transport::CTRL_RING_CAPACITY
    >;

} // namespace wirekrak::core::preset
