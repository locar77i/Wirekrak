
#include "wirekrak/core/transport/websocket/data_block.hpp"
#include "wirekrak/core/config/transport/websocket.hpp"
#include "lcr/lockfree/spsc_ring.hpp"


namespace wirekrak::core::preset {

    using DefaultMessageRing =
        lcr::lockfree::spsc_ring<
        wirekrak::core::transport::websocket::DataBlock,
        wirekrak::core::transport::RX_RING_CAPACITY
    >;

} // namespace wirekrak::core::preset
