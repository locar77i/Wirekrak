#pragma once

#include "lcr/buffer/managed_spsc_ring.hpp"
#include "lcr/buffer/managed_slot.hpp"
#include "lcr/buffer/concepts.hpp"
#include "lcr/memory/block_pool.hpp"


namespace wirekrak::core::preset {

    // -------------------------------------------------------------------------
    // Managed SPSC ring setup
    // -------------------------------------------------------------------------
    using DefaultMessageRing =
        lcr::buffer::managed_spsc_ring<
            lcr::buffer::managed_slot<config::transport::websocket::MIN_FRAME_SIZE>,
            lcr::memory::block_pool,
            config::transport::MESSAGE_RING_CAPACITY
        >;

    // Assert that DefaultMessageRing satisfies the ManagedSpscRingConcept
    static_assert(lcr::buffer::ManagedSpscRingConcept<DefaultMessageRing>, "DefaultMessageRing does not satisfy ManagedSpscRingConcept");

} // namespace wirekrak::core::preset
