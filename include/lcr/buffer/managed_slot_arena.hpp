#pragma once

/*
===============================================================================
managed_slot
===============================================================================

Hybrid streaming slot optimized for small-message dominance.

Design
------
  • Owned buffer (fast path)
  • Optional external pooled block (promotion path)
  • No heap allocations after construction
  • Deterministic lifetime
  • Compatible with SPSC ring buffers
  • Incremental write API (reserve → write_ptr → commit)

Threading Model
---------------
  • Producer writes
  • Consumer reads
  • No concurrent mutation

Promotion Model
---------------
  • owned → external (one-way)
  • Never demotes
  • Consumer must call reset(pool) to release external block

===============================================================================
*/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cassert>

#include "lcr/memory/block_pool.hpp"
#include "lcr/buffer/concepts.hpp"
#include "lcr/trap.hpp"



namespace lcr::buffer {


static inline constexpr std::size_t DEFAULT_INLINE_SIZE = 256;
static inline constexpr std::size_t DEFAULT_MAX_RESERVE_SIZE = 1 << 20; // 1 MiB

enum class PromotionResult : std::uint8_t {
    None = 0,         // No promotion required
    Success = 1,      // Promotion performed
    PoolExhausted = 2,
    TooLarge = 3
};

template <
    std::size_t InlineSize = DEFAULT_INLINE_SIZE,
    std::size_t MaxReserveSize = DEFAULT_MAX_RESERVE_SIZE
>
class managed_slot {
public:

    using promotion_result_type = PromotionResult;

    static constexpr std::size_t inline_size = InlineSize;

public:
    managed_slot() noexcept = default;
    ~managed_slot() noexcept = default;
    
    managed_slot(const managed_slot&) = delete;
    managed_slot& operator=(const managed_slot&) = delete;

    // --------------------------------------------------------------------------
    // Initialization: called once by ring
    // --------------------------------------------------------------------------

    void init(char* chunk) noexcept {
        buffer_ = chunk;
    }

    // -------------------------------------------------------------------------
    // Basic Accessors
    // -------------------------------------------------------------------------

    [[nodiscard]]
    inline char* data() noexcept {
        return external_ ? external_->data() : buffer_;
    }

    [[nodiscard]]
    inline const char* data() const noexcept {
        return external_ ? external_->data() : buffer_;
    }

    [[nodiscard]]
    inline std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]]
    inline std::size_t capacity() const noexcept {
        return external_ ? external_->capacity() : InlineSize;
    }

    [[nodiscard]]
    inline bool is_external() const noexcept {
        return external_ != nullptr;
    }

    [[nodiscard]]
    inline std::size_t remaining() const noexcept {
        return capacity() - size();
    }

    // -------------------------------------------------------------------------
    // Write Cursor
    // -------------------------------------------------------------------------

    /*
        Returns pointer to current write position.

        PRECONDITION:
            reserve() must have succeeded for the intended write size.
    */
    [[nodiscard]]
    inline char* write_ptr() noexcept {
        return data() + size_;
    }

    // -------------------------------------------------------------------------
    // Reserve Capacity
    // -------------------------------------------------------------------------

    /*
        Ensures capacity for writing `len` additional bytes.

        Returns:
            None          → already sufficient inline capacity
            Success       → promotion occurred
            PoolExhausted → external block unavailable
            TooLarge      → exceeds MaxReserveSize or overflow detected
    */
    [[nodiscard]]
    inline PromotionResult reserve(std::size_t len, memory::block_pool& pool) noexcept {
        if (len > MaxReserveSize)
            return PromotionResult::TooLarge;

        const std::size_t required = size_ + len;

        // Overflow guard
        if (required < size_ || required > MaxReserveSize)
            return PromotionResult::TooLarge;

        // Already enough capacity
        if (required <= capacity())
            return PromotionResult::None;

        return promote_if_needed_(required, pool);
    }

    // -------------------------------------------------------------------------
    // Commit
    // -------------------------------------------------------------------------

    /*
        Advances write cursor by len bytes.

        PRECONDITION:
            reserve(len) must have succeeded.
    */
    inline std::size_t commit(std::size_t len) noexcept {
        LCR_ASSERT_MSG(size_ + len <= capacity(), "Commit size exceeds capacity");

        size_ += len;

        if (external_) {
            external_->set_size(size_);
        }

        return size_;
    }

    // -------------------------------------------------------------------------
    // Timestamp
    // -------------------------------------------------------------------------

    inline void set_timestamp(std::uint64_t ts) noexcept {
        timestamp_ns_ = ts;
    }

    inline void inc_timestamp(std::uint64_t delta) noexcept {
        timestamp_ns_ += delta;
    }

    [[nodiscard]]
    inline std::uint64_t timestamp() const noexcept {
        return timestamp_ns_;
    }

    void reset_timestamp() noexcept {
        timestamp_ns_ = 0;
    }

    // -------------------------------------------------------------------------
    // Reset (Consumer side)
    // -------------------------------------------------------------------------

    /*
        Releases external memory (if any) and resets slot.

        Must be called by managed_spsc_ring on consumer release.
    */
    inline void reset(memory::block_pool& pool) noexcept {
        if (external_) {
            pool.release(external_);
            external_ = nullptr;
        }

        size_ = 0;
        timestamp_ns_ = 0;
    }

private:
    memory::block* external_{nullptr};
    char* buffer_{nullptr};
    std::size_t size_{0};
    std::uint64_t timestamp_ns_{0};

private:

    // Promote inline buffer to external block if required
    [[nodiscard]]
    inline PromotionResult promote_if_needed_(std::size_t required, memory::block_pool& pool) noexcept {
        if (required <= InlineSize)
            return PromotionResult::None;

        if (!external_) {
            memory::block* block = pool.acquire();
            if (!block)
                return PromotionResult::PoolExhausted;

            std::memcpy(block->data(), buffer_, size_);
            block->set_size(size_);
            external_ = block;
        }

        return required <= capacity()
            ? PromotionResult::Success
            : PromotionResult::TooLarge;
    }
};

// Assert that managed_slot satisfies the ManagedSlotConcept
static_assert(ManagedSlotConcept<managed_slot<>, memory::block_pool>, "managed_slot does not satisfy ManagedSlotConcept");

} // namespace lcr::buffer
