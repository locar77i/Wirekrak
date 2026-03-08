#pragma once

#include <concepts>
#include <cstddef>


namespace lcr::buffer {


template<typename Slot>
concept ProducerSlotConcept =
    std::default_initializable<Slot>
    &&
    requires(
        Slot& slot,
        std::size_t len
    )
{
    typename Slot::promotion_result_type;

    // Remaining writable capacity
    { slot.remaining() } noexcept -> std::same_as<std::size_t>;

    // Write cursor
    { slot.write_ptr() } noexcept -> std::same_as<char*>;

    // Commit written bytes
    { slot.commit(len) } noexcept -> std::same_as<std::size_t>;
};


template<typename Slot>
concept ConsumerSlotConcept =
    std::default_initializable<Slot>
    &&
    requires(Slot& slot)
{
    // Data access
    { slot.data() } noexcept -> std::same_as<char*>;

    // Size access
    { slot.size() } noexcept -> std::same_as<std::size_t>;
};


template<typename Slot, typename MemoryPool>
concept ManagedSlotConcept = ConsumerSlotConcept<Slot> && ProducerSlotConcept<Slot>
    &&
    requires(Slot& slot,
        MemoryPool& pool
    )
{
    // Deterministic cleanup
    { slot.reset(pool) } noexcept -> std::same_as<void>;
};


/*
===============================================================================
ConsumerSpscRingConcept
===============================================================================

Defines the minimal contract required for a Single-Producer / Single-Consumer
ring buffer when used strictly as a *consumer-side* message stream.

This concept is intended for higher-level components (e.g. protocol sessions)
that:

  • Consume fully assembled messages
  • Do NOT perform producer-side operations
  • Do NOT depend on incremental write semantics
  • Do NOT depend on memory promotion logic

This keeps architectural layering clean and prevents over-constraining
higher layers to producer-specific APIs.

-------------------------------------------------------------------------------

Design Principles
-----------------
  • Constrain only what is used
  • Preserve substitution flexibility
  • Avoid coupling to memory pool or promotion semantics
  • Remain compatible with lock-free SPSC semantics

-------------------------------------------------------------------------------

Threading Model Assumptions
---------------------------
  • Exactly one producer thread
  • Exactly one consumer thread
  • Consumer calls:
        peek_consumer_slot()
        release_consumer_slot()
  • No concurrent mutation

===============================================================================
*/
template<typename Ring>
concept ConsumerSpscRingConcept =
    requires(Ring& ring)
{
    typename Ring::slot_type;
    requires ConsumerSlotConcept<typename Ring::slot_type>;

    // Obtain readable slot (nullptr if empty)
    { ring.peek_consumer_slot() } noexcept -> std::same_as<typename Ring::slot_type*>;

    // Release previously consumed slot
    { ring.release_consumer_slot() } noexcept -> std::same_as<void>;

    // Lifecycle: clear ring (must not throw)
    { ring.clear() } noexcept -> std::same_as<void>;
};


/*
===============================================================================
ProducerSpscRingConcept
===============================================================================

Defines the minimal producer-side contract required for a
Single-Producer / Single-Consumer (SPSC) ring buffer.

This concept is intended for components that act strictly as producers
(e.g. WebSocket transport receive loops) and therefore must NOT depend
on any consumer-side functionality.

It prevents over-constraining higher layers by ensuring that only the
producer interface is required.

-------------------------------------------------------------------------------

Design Philosophy
-----------------
• Enforces explicit producer lifecycle control
• Supports incremental slot-based writing
• Supports promotion-aware reserve semantics
• Requires noexcept operations (ULL-safe)
• Does NOT require consumer operations
• Does NOT require introspection APIs

-------------------------------------------------------------------------------

Threading Model Assumptions
---------------------------
• Exactly one producer thread
• Exactly one consumer thread (not visible here)
• This concept validates ONLY producer-facing operations
• All operations must be noexcept

===============================================================================
*/
template<typename Ring>
concept ProducerSpscRingConcept =
    requires(
        Ring& ring,
        typename Ring::slot_type* slot,
        std::size_t len
    )
{
    typename Ring::slot_type;
    requires ProducerSlotConcept<typename Ring::slot_type>;
    typename Ring::promotion_result_type;

    // Acquire writable slot (may return nullptr if full)
    { ring.acquire_producer_slot() } noexcept -> std::same_as<typename Ring::slot_type*>;

    // Reserve additional writable capacity (incremental write support)
    { ring.reserve(slot, len) } noexcept -> std::same_as<typename Ring::promotion_result_type>;

    // Publish the slot (makes it visible to the consumer)
    { ring.commit_producer_slot() } noexcept -> std::same_as<void>;

    // Discard a producer-acquired slot without publishing it
    { ring.discard_producer_slot(slot) } noexcept -> std::same_as<void>;
};


/*
===============================================================================
ManagedSpscRingConcept
===============================================================================

Defines the required interface for a Single-Producer / Single-Consumer
streaming ring buffer compatible with incremental slot writing and
deterministic release semantics.

This concept allows higher layers (e.g. WebSocket transport) to depend
only on ring capabilities — not on concrete implementation.

The ring must:

  • Provide explicit producer lifecycle control
  • Support incremental slot reserve semantics
  • Support deterministic consumer release
  • Be SPSC-safe
  • Be noexcept for all core operations

===============================================================================
*/
template<typename Ring>
concept ManagedSpscRingConcept = ProducerSpscRingConcept<Ring> && ConsumerSpscRingConcept<Ring>
    &&
    requires(Ring& ring)
{
    // Optional introspection APIs
    { ring.empty() } noexcept -> std::same_as<bool>;
    { ring.full() } noexcept -> std::same_as<bool>;
    { ring.used() } noexcept -> std::same_as<std::size_t>;
    { ring.capacity() } noexcept -> std::same_as<std::size_t>;
};

} // namespace lcr::buffer
