#pragma once

/*
===============================================================================
managed_spsc_ring
===============================================================================

High-level managed Single-Producer / Single-Consumer ring buffer built
on top of lcr::lockfree::spsc_ring.

Purpose
-------
Adds deterministic memory management to a lock-free SPSC ring for slot
types that require an external memory pool (e.g. managed_slot with
promotion support).

This wrapper:

  • Delegates queue mechanics to spsc_ring
  • Injects an external MemoryPool (non-owning)
  • Automatically resets slots on consumer release
  • Hides memory_pool from higher layers
  • Preserves explicit producer lifecycle control

-------------------------------------------------------------------------------

Design Philosophy
-----------------
  • No dynamic allocations
  • No ownership of MemoryPool
  • No hidden producer staging state
  • No additional synchronization
  • Deterministic lifetime control
  • ULL-safe

-------------------------------------------------------------------------------

Threading Model
---------------
Exactly one producer thread.
Exactly one consumer thread.

Producer:
    Slot* slot = ring.acquire_producer_slot();
    if (!slot) { *ring full* }

    auto r = ring.reserve(slot, len);
    if (r != PromotionResult::None &&
        r != PromotionResult::Success) {
        *pool exhausted or too large*
    }

    char* dst = slot->write_ptr();
    slot->commit(bytes);

    ring.commit_producer_slot();

Consumer:
    Slot* slot = ring.peek_consumer_slot();
    process(slot->data(), slot->size());
    ring.release_consumer_slot();  // calls slot.reset(pool_)

-------------------------------------------------------------------------------

Important Guarantees
--------------------
• reserve() forwards pool internally — transport never sees pool
• release_consumer_slot() automatically calls slot.reset(pool_)
• MemoryPool must outlive this ring
• No slot must be accessed after release

===============================================================================
*/

#include <cstddef>
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/buffer/concepts.hpp"


namespace lcr::buffer {

template<
    typename Slot,
    typename MemoryPool,
    std::size_t Capacity
>
requires ManagedSlotConcept<Slot, MemoryPool>
class managed_spsc_ring {
public:

    using slot_type = Slot;
    using pool_type = MemoryPool;
    using promotion_result_type = typename Slot::promotion_result_type;

    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------

    explicit managed_spsc_ring(MemoryPool& pool) noexcept
        : pool_(pool)
    {}

    managed_spsc_ring(const managed_spsc_ring&) = delete;
    managed_spsc_ring& operator=(const managed_spsc_ring&) = delete;

    // =========================================================================
    // Producer API
    // =========================================================================

    /*
        Acquire writable slot.

        Returns:
            Pointer to slot if space available.
            nullptr if ring is full.

        Producer must:
            1. Write into slot
            2. Call commit_producer_slot()
    */
    [[nodiscard]]
    inline Slot* acquire_producer_slot() noexcept {
        return ring_.acquire_producer_slot();
        Slot* slot = ring_.acquire_producer_slot();
        if (slot) [[likely]] { // Ensure slot is always clean when handed to producer
            slot->reset(pool_);
        }
        return slot;
    }

    /*
        Commit previously acquired producer slot.

        Makes the written message visible to consumer.
    */
    inline void commit_producer_slot() noexcept {
        ring_.commit_producer_slot();
    }

    /*
        Reserve writable capacity inside the slot.

        Forwards MemoryPool internally.

        Returns:
            PromotionResult::None          → no promotion required
            PromotionResult::Success       → promotion performed
            PromotionResult::PoolExhausted → no external block available
            PromotionResult::TooLarge      → exceeds slot limits
    */
    [[nodiscard]]
    inline promotion_result_type reserve(Slot* slot, std::size_t len ) noexcept {
#ifndef NDEBUG
        assert(slot && "reserve() called with null slot");
#endif
        return slot->reserve(len, pool_);
    }

    /*
        Discard a producer-acquired slot that will not be committed.

        This method must be used when the producer abandons a slot after it has
        already been acquired but before commit_producer_slot() is called.

        Behavior:
            • Calls slot.reset(pool_) to return any external memory to the pool
            • Clears slot state so that it can be safely reused by the producer

        Important: The slot must NOT have been committed
    */
    inline void discard_producer_slot(Slot* slot) noexcept {
        if (slot) [[likely]] {
            slot->reset(pool_);
        }
    }

    // =========================================================================
    // Consumer API
    // =========================================================================

    /*
        Peek readable slot.

        Returns:
            Pointer to slot if available.
            nullptr if ring is empty.
    */
    [[nodiscard]]
    inline Slot* peek_consumer_slot() noexcept {
        return ring_.peek_consumer_slot();
    }

    /*
        Release previously consumed slot.

        Behavior:
            • Calls slot.reset(pool_)
            • Returns any external memory to pool
            • Advances consumer index

        IMPORTANT:
            Slot must not be accessed after this call.
    */
    inline void release_consumer_slot() noexcept {
        Slot* slot = ring_.peek_consumer_slot();
        if (slot) {
            slot->reset(pool_);
        }

        ring_.release_consumer_slot();
    }

    // =========================================================================
    // Introspection
    // =========================================================================

    [[nodiscard]]
    inline bool empty() const noexcept {
        return ring_.empty();
    }

    [[nodiscard]]
    inline bool full() const noexcept {
        return ring_.full();
    }

    [[nodiscard]]
    inline std::size_t used() const noexcept {
        return ring_.used();
    }

    [[nodiscard]]
    inline constexpr std::size_t capacity() const noexcept {
        return ring_.capacity();
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /*
        Clear ring and release all external memory.

        PRECONDITION:
            • Producer thread must be stopped
            • No concurrent access
    */
    inline void clear() noexcept {
        while (auto* slot = ring_.peek_consumer_slot()) {
            slot->reset(pool_);
            ring_.release_consumer_slot();
        }
    }

    // =========================================================================
    // Memory Footprint
    // =========================================================================

    [[nodiscard]]
    inline auto memory_usage() const noexcept {
        return ring_.memory_usage();
    }

private:

    // Underlying lock-free SPSC ring
    lockfree::spsc_ring<Slot, Capacity> ring_;

    // Shared memory pool (non-owning)
    MemoryPool& pool_;
};

} // namespace lcr::buffer
