#pragma once

/*
===============================================================================
block_pool (Lock-Free, Fixed-Capacity, Reusable Buffer Pool)
===============================================================================

Purpose
-------
Provides a lock-free pool of preallocated memory block objects.

Designed for:
  • Ultra-low-latency environments
  • Zero runtime allocations
  • Deterministic memory footprint
  • Multi-threaded producer/consumer usage
  • Reuse across multiple sessions

Key Properties
--------------
  • All buffers allocated at construction
  • No allocation or deallocation during steady-state
  • Lock-free (Treiber stack free-list)
  • Fixed capacity
  • Pool exhaustion detectable (acquire() returns nullptr)

Threading Model
---------------
  • Multiple threads may call acquire() and release()
  • Uses atomic CAS on a singly-linked free list
  • No locks, no blocking

ABA Safety
----------
ABA is not a concern here because:
  • Nodes are never destroyed during runtime
  • Memory lifetime equals pool lifetime
  • No reclamation occurs

===============================================================================
*/

#include <atomic>
#include <cstddef>
#include <cassert>

#include "lcr/memory/block.hpp"
#include "lcr/memory/footprint.hpp"
#include "lcr/trap.hpp"


namespace lcr::memory {

class block_pool {
public:
    /*
    ---------------------------------------------------------------------------
    Constructor
    ---------------------------------------------------------------------------

    block_size  : size of each individual memory block
    block_count : number of blocks to preallocate

    All memory is allocated here.
    After construction, the pool performs no further heap allocations.
    */
    block_pool(std::size_t block_size, std::size_t block_count)
        : block_size_(block_size)
        , block_count_(block_count)
    {
        // Allocate raw memory for N nodes (unconstructed)
        blocks_ = static_cast<node*>(
            ::operator new[](block_count_ * sizeof(node))
        );
        // Construct each memory block in-place and push into free-list
        for (std::size_t i = 0; i < block_count_; ++i) {
            node* n = &blocks_[i];
            // Placement-new constructs memory block inside node
            new (&n->block) block(block_size_);
            // Add node to free-list (lock-free)
            push_node_(n);
        }
        // Initialize free count
        free_count_.store(block_count_, std::memory_order_relaxed);
    }

    /*
    ---------------------------------------------------------------------------
    Destructor
    ---------------------------------------------------------------------------

    Destroys all memory_blocks and frees backing storage.

    Assumption:
      No thread is using the pool at destruction time.
    */
    ~block_pool() noexcept {
        for (std::size_t i = 0; i < block_count_; ++i) {
            blocks_[i].block.~block();
        }
        ::operator delete[](blocks_);
    }

    block_pool(const block_pool&) = delete;
    block_pool& operator=(const block_pool&) = delete;

    /*
    ---------------------------------------------------------------------------
    acquire()
    ---------------------------------------------------------------------------

    Attempts to pop one block from the free-list.

    Returns:
      • pointer to memory block if available
      • nullptr if pool exhausted

    Lock-free O(1)
    */
    [[nodiscard]]
    block* acquire() noexcept {
        node* n = pop_node_();
        if (!n) [[unlikely]] {
            return nullptr; // pool exhausted
        }
        // Successfully popped a node, decrement free count
        free_count_.fetch_sub(1, std::memory_order_relaxed);
        // Reset logical size before reuse
        n->block.reset();
        return &n->block;
    }

    /*
    ---------------------------------------------------------------------------
    release()
    ---------------------------------------------------------------------------

    Returns a previously acquired block to the pool.

    IMPORTANT:
      Caller must guarantee the block belongs to this pool.
    */
    void release(block* block) noexcept {
        LCR_ASSERT_MSG(block, "release() called with null block");
        node* n = node_from_block_(block);
        push_node_(n);
        // Increment free count after push to ensure visibility of the released block
        free_count_.fetch_add(1, std::memory_order_relaxed);
    }


    // =========================================================================
    // Introspection
    // =========================================================================

    [[nodiscard]]
    std::size_t used() const noexcept {
        return block_count_ - free_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]]
    constexpr std::size_t capacity() const noexcept {
        return block_count_;
    }

    // ---------------------------------------------------------------------------
    // Memory Usage Introspection
    // ---------------------------------------------------------------------------
    [[nodiscard]]
    footprint memory_usage() const noexcept {
        footprint fp;
        fp.add_static(sizeof(*this));
        fp.add_dynamic(block_count_ * sizeof(node));
        for (std::size_t i = 0; i < block_count_; ++i) {
            fp.add_dynamic(blocks_[i].block);
        }
        return fp;
    }

private:
    /*
    ===========================================================================
    Internal Node Structure
    ===========================================================================

    Each node contains:
      • block
      • next pointer (for free-list linkage)

    This makes the free-list intrusive:
      The nodes themselves store linkage.
    */
    struct node {
        block block; // placement-constructed later
        node* next{nullptr};
    };

    // Head of lock-free stack (Treiber stack). Points to the first free node.
    alignas(64) std::atomic<node*> head_{nullptr};

    node* blocks_{nullptr};    // Backing storage for all nodes
    std::size_t block_size_;   // Size of each memory block
    std::size_t block_count_;  // Total number of blocks in the pool

    alignas(64) std::atomic<std::size_t> free_count_;  // Tracks number of free blocks (for introspection)

private:
    /*
    ===========================================================================
    Lock-Free Push (Treiber Stack)
    ===========================================================================

    Algorithm:
      1. Read current head
      2. Link new node to old head
      3. CAS head from old_head to new node
      4. Retry on failure

    Memory Ordering:
      • release on success ensures writes to node->next are visible
      • relaxed on failure is sufficient
    */
    void push_node_(node* n) noexcept {
        node* old_head = head_.load(std::memory_order_relaxed);
        do {
            n->next = old_head;
        } while (!head_.compare_exchange_weak(
            old_head,
            n,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    /*
    ===========================================================================
    Lock-Free Pop
    ===========================================================================

    Algorithm:
      1. Load head
      2. Read next pointer
      3. CAS head to next
      4. Retry if CAS fails

    Memory Ordering:
      • acquire ensures visibility of prior writes to node
      • relaxed on failure

    Returns nullptr if pool empty.
    */
    [[nodiscard]]
    node* pop_node_() noexcept {
        node* old_head = head_.load(std::memory_order_acquire);
        while (old_head) {
            node* next = old_head->next;
            if (head_.compare_exchange_weak(
                    old_head,
                    next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed))
            {
                return old_head;
            }
        }
        return nullptr;
    }

    /*
    ===========================================================================
    node_from_block_
    ===========================================================================

    Converts block* back to its containing node*.

    Because block is first member inside node, pointer arithmetic
    safely retrieves enclosing structure.

    This relies on standard layout guarantees.
    */
    [[nodiscard]]
    node* node_from_block_(block* block) noexcept {
        return reinterpret_cast<node*>(
            reinterpret_cast<char*>(block) - offsetof(node, block)
        );
    }

};

} // namespace lcr::memory
