
// =============================================================================
//  HYBRID HOT/COLD PARTITIONING DESIGN OVERVIEW
// =============================================================================
//
//  Motivation
//  ----------
//  The Matching Engine (ME) previously used a dense, preallocated array of
//  Partitions, each containing a contiguous array of PriceLevels.  This design
//  guarantees O(1) access and perfect cache locality, but it consumes large
//  amounts of memory because most partitions remain almost empty during normal
//  market conditions.
//
//  In production exchanges (CME, NASDAQ, Kraken, etc.), the vast majority of
//  activity is concentrated near the *current market price* (within a few
//  hundred ticks).  Far-away price regions are cold: they receive no updates
//  and can be stored in a much more compact form.  This design introduces a
//  two-tier "hybrid" storage model to exploit this property.
//
// -----------------------------------------------------------------------------
//  Architecture Summary
// -----------------------------------------------------------------------------
//
//   ┌─────────────────────┐        promote()        ┌──────────────────────┐
//   │   ColdPartition     │ --------------------->  │      Partition       │
//   │ (sparse, lightweight│                         │ (dense, hot, O(1)    │
//   │  only active levels)│ <---------------------  │  access per level)   │
//   └─────────────────────┘        demote()         └──────────────────────┘
//
//  - The Matching Engine *never* allocates or frees partitions.
//    It simply operates on currently-active (hot) partitions.
//
//  - A background orchestrator thread tracks the last traded price and
//    maintains a "heat window" around it (±N partitions).
//
//  - When the market moves, partitions that enter the window are promoted
//    (Cold → Hot), and partitions that leave it are demoted (Hot → Cold).
//
//  - Promotion and demotion are symmetric and deterministic; no heap
//    allocations occur on the hot path.
//
// -----------------------------------------------------------------------------
//  Classes
// -----------------------------------------------------------------------------
//
//  class PriceLevel
//  ----------------
//  Represents one price level and is shared between hot and cold tiers.
//  Holds price, total quantity, intrusive list head/tail, and an active flag.
//  This reuse keeps conversions trivial and metrics uniform.
//
//  class ColdPartition
//  -------------------
//  - Holds a small vector of active PriceLevels only.
//  - No bitmaps or preinitialized arrays.
//  - Used for cold / inactive price regions outside the heat window.
//  - Cheap in memory (typically < 128 active levels).
//
//  class Partition (existing)
//  --------------------------
//  - Dense, cache-aligned array of PriceLevels covering a fixed price range.
//  - Provides O(1) lookup by tick index.
//  - Used for hot regions where orders and trades are concentrated.
//
// -----------------------------------------------------------------------------
//  Promotion / Demotion
// -----------------------------------------------------------------------------
//
//  Partition::swap_with_cold(ColdPartition&& incoming)
//  ---------------------------------------------------
//  Promotes the given cold partition into this slot and returns the previous
//  hot partition demoted to cold form.  The method:
//
//   1. Extracts current active PriceLevels → builds a ColdPartition.
//   2. Clears and reinitializes this Partition for the new partid.
//   3. Copies all PriceLevels from the incoming cold partition into the
//      dense structure.
//   4. Returns the extracted ColdPartition for later reuse or persistence.
//
//  All operations are O(number_of_active_levels) and noexcept.
//
// -----------------------------------------------------------------------------
//  Background Orchestrator
// -----------------------------------------------------------------------------
//
//  - Runs in its own thread or core.
//
//  - Receives the *latest* "last traded price" from the ME via a single
//    atomic variable (std::atomic<Price>).  This replaces the earlier SPSC
//    ring buffer idea — since only the most recent value matters, old prices
//    are obsolete and no queueing is necessary.
//
//  - The orchestrator periodically polls this atomic value and determines
//    which partition ID corresponds to the current market price.
//
//  - Maintains the center partition corresponding to the last price and a
//    configurable radius N defining the heat window.
//
//  - Applies a *hysteresis margin* (H partitions) to prevent oscillation:
//    the center only recenters if the last traded price moves more than
//    H partitions beyond the current window boundary.
//
//  - On a valid recenter event, computes which partitions must be promoted
//    or demoted and calls Partition::swap_with_cold() accordingly.
//
//  The ME itself never performs promotion/demotion decisions or memory
//  management.  It remains purely deterministic and real-time safe.
//
// -----------------------------------------------------------------------------
//  Advantages
// -----------------------------------------------------------------------------
//
//  - Predictable latency: no heap allocations or background interference.
//  - Predictable memory footprint: at most (2N + 1) hot partitions per side.
//  - Improved cache efficiency and NUMA locality.
//  - Stability through hysteresis: avoids thrashing when prices oscillate
//    near partition boundaries.
//  - Clean separation of responsibilities:
//      * Matching Engine → trade & order processing
//      * Orchestrator    → memory / heat management
//      * PartitionPool   → allocation and recycling
//
// -----------------------------------------------------------------------------
//  Future Extensions
// -----------------------------------------------------------------------------
//
//  - ColdPartitionPool: preallocates and recycles ColdPartition objects to
//    avoid even small-vector allocations.
//
//  - Adaptive heat radius: N can grow/shrink based on volatility metrics.
//
//  - Adaptive hysteresis: H can widen during volatile periods and shrink
//    during calm markets.
//
//  - Asymmetric heat windows for BID and ASK sides.
//
//  - Persistent cold partition store: serialize rarely used levels to disk
//    or slower memory tiers.
//
// -----------------------------------------------------------------------------
//  Implementation Notes
// -----------------------------------------------------------------------------
//
//  - Both Partition and ColdPartition reuse the same PriceLevel type for now.
//    This keeps the interface symmetric; if memory becomes an issue, a
//    specialized ColdPriceLevel may later be introduced.
//
//  - All promotion/demotion work is off the ME's critical path and should be
//    performed asynchronously by the orchestrator.
//
//  - The orchestrator reads the last price atomically; this eliminates any
//    risk of stale-queue buildup and minimizes cache contention.
//
//  - The PartitionPool continues to own memory for all active partitions.
//
// =============================================================================




// =============================================================================
//  PRICE LEVEL STORE AS THE UNIFIED MEMORY LAYOUT MANAGER
// =============================================================================
//
//  Motivation
//  ----------
//  The Matching Engine (ME) must remain deterministic and ultra-low latency,
//  focusing purely on trading logic (insert, match, cancel, modify) without
//  being aware of how price levels are physically stored in memory.
//
//  However, the underlying data (orders grouped by price) can reside in
//  different *storage tiers* depending on activity:
//      - "Hot" regions near the market price → dense, cache-aligned partitions.
//      - "Cold" regions far away → sparse partitions backed by pools.
//
//  The PriceLevelStore is the abstraction boundary that isolates the ME from
//  those memory layout details and transparently manages where and how data
//  is stored.
//
// -----------------------------------------------------------------------------
//  Role in the Architecture
// -----------------------------------------------------------------------------
//
//      MatchingEngine
//           │
//           ▼
//      ┌────────────┐
//      │ OrderBook  │
//      └─────┬──────┘
//            ▼
//      ┌────────────┐
//      │ PriceLevel │───→ decides where a price lives (hot/cold)
//      │   Store    │
//      └────────────┘
//           │
//     ┌─────┴──────────────────────────────────────────┐
//     │                                                │
// ┌───▼────────────┐                         ┌──────────▼───────────┐
// │  Hot Partitions│  dense arrays, O(1)     │   Cold Partitions    │
// │ (active region)│  access, per-tick index │ (sparse, intrusive)  │
// └────────────────┘                         └──────────────────────┘
//
// -----------------------------------------------------------------------------
//  Responsibilities
// -----------------------------------------------------------------------------
//
//  1. **Price Routing**
//     - Maps any order’s price → PartitionId.
//     - Decides whether that partition is currently hot or cold.
//
//  2. **Unified Access Interface**
//     - Exposes methods for insert, modify, cancel, and match that behave
//       identically regardless of underlying storage type.
//     - Internally forwards to either a `Partition` (dense) or `ColdPartition`
//       (sparse) implementation.
//
//  3. **Memory Management**
//     - Allocates partitions from `PartitionPool` (hot) and
//       `ColdPartitionPool` (cold) as needed.
//     - Never performs heap allocations during runtime.
//     - Frees or swaps partitions deterministically during demotion/promotion.
//
//  4. **Promotion / Demotion Integration**
//     - Receives “heat window” updates from the orchestrator thread.
//     - When a partition crosses the hot/cold boundary:
//         * Calls `swap_with_cold()` on the `Partition`.
//         * Updates internal routing tables.
//     - ME itself is unaware of this transition.
//
//  5. **Metrics and Telemetry**
//     - Tracks partition occupancy, active level count, best price, etc.
//     - Reports memory footprint per side (BID / ASK).
//
// -----------------------------------------------------------------------------
//  Core Data Members (Conceptual)
// -----------------------------------------------------------------------------
//
//      struct PartitionEntry {
//          PartitionId id;
//          bool is_hot;
//          IPartition* ptr;         // points to either Partition or ColdPartition
//      };
//
//      std::vector<PartitionEntry> partitions_;
//      PartitionPool hot_pool_;
//      ColdPartitionPool cold_pool_;
//
// -----------------------------------------------------------------------------
//  Example Access Path
// -----------------------------------------------------------------------------
//
//      void PriceLevelStore::insert_order(const Order& order) noexcept {
//          PartitionId pid = partition_for_price(order.price);
//
//          PartitionEntry& entry = partitions_[pid];
//          if (!entry.ptr) {
//              if (is_hot_price(order.price))
//                  entry.ptr = hot_pool_.allocate(pid);
//              else
//                  entry.ptr = cold_pool_.allocate(pid);
//              entry.is_hot = is_hot_price(order.price);
//          }
//
//          PriceLevel* lvl = entry.ptr->find_or_create_level(order.price);
//          lvl->add_quantity(order.qty);
//      }
//
//  The ME calls this function without knowing whether the operation occurs
//  in hot or cold memory — the behavior is identical from its perspective.
//
// -----------------------------------------------------------------------------
//  Advantages
// -----------------------------------------------------------------------------
//
//  - **Layer isolation:** ME logic is completely agnostic to memory layout.
//  - **Predictable latency:** all allocations handled via preallocated pools.
//  - **Dynamic adaptability:** partitions transparently move between hot/cold.
//  - **Memory efficiency:** only hot regions are stored densely in RAM.
//  - **Maintainability:** future tiers (semi-hot, persistent) can be added
//    without touching the ME or OrderBook.
//
// -----------------------------------------------------------------------------
//  Future Extensions
// -----------------------------------------------------------------------------
//
//  - Adaptive partition sizing based on volatility or order flow.
//  - NUMA-aware partition placement for multi-core engines.
//  - Real-time telemetry hooks for hot/cold partition usage.
//  - Tiered memory (L3 cache → DRAM → PMEM) integration.
//
// =============================================================================



// =============================================================================
//  CONCURRENCY DESIGN: MATCHING ENGINE VS BACKGROUND PARTITION MANAGER
// =============================================================================
//
//  Motivation
//  ----------
//  The Matching Engine (ME) operates continuously on the live OrderBook data
//  (PriceLevelStores → Partitions → PriceLevels), processing millions of
//  messages per second.  Simultaneously, a background thread (the "Partition
//  Orchestrator") promotes and demotes partitions between hot and cold zones
//  to optimize memory and cache locality.
//
//  Both threads access and occasionally modify the same logical structures,
//  so the design must guarantee:
//
//    • ZERO blocking in the ME (no locks, no stalls, no memory fences beyond
//      atomic pointer swaps).
//    • Deterministic behavior (no torn reads or partial visibility).
//    • Eventual consistency between ME’s view and the background worker’s
//      view of partitions.
//    • Safe reclamation of demoted partitions.
//
// -----------------------------------------------------------------------------
//  Actors
// -----------------------------------------------------------------------------
//
//   (1) Matching Engine (ME)
//       - Single thread.
//       - Performs order inserts, cancels, modifies, and trades.
//       - Accesses OrderBook → PriceLevelStore → active Partitions.
//       - Must never block or synchronize on background operations.
//
//   (2) Background Partition Orchestrator (Worker)
//       - Single thread (dedicated core recommended).
//       - Receives “last traded price” updates from the ME.
//       - Continuously adjusts which partitions are hot vs cold by promoting
//         and demoting them.
//       - Performs atomic swaps to replace cold/hot partitions in memory.
//
// -----------------------------------------------------------------------------
//  Concurrency Constraints
// -----------------------------------------------------------------------------
//
//  The ME and Worker both read and write shared partition structures.  To
//  prevent race conditions:
//
//   • Each Partition pointer in the PriceLevelStore is atomic.
//   • The ME always dereferences these pointers atomically (no caching).
//   • Structural changes (promotion/demotion) are made by atomically swapping
//     entire Partition pointers, never by mutating data in place.
//   • Fine-grained version counters detect mid-copy interference.
//
// -----------------------------------------------------------------------------
//  1. Versioned Atomic Swap Protocol
// -----------------------------------------------------------------------------
//
//  Each Partition (hot or cold) carries a 64-bit atomic version counter.
//
//   struct Partition {
//       std::atomic<uint64_t> version;
//       ...
//   };
//
//  The ME increments the version before and after any structural mutation
//  (insert, cancel, etc.).  The background worker uses this version to verify
//  stability when copying partitions.
//
//   ColdPartition copy_partition_safely(const Partition* src) {
//       uint64_t v0 = src->version.load(std::memory_order_acquire);
//       copy_data(src, tmp);
//       uint64_t v1 = src->version.load(std::memory_order_acquire);
//       if (v0 != v1) return {}; // ME modified during copy → abort & retry
//       return tmp;
//   }
//
//  Once a consistent snapshot is obtained, the worker builds the replacement
//  (promoted or demoted) partition and performs:
//
//       partition_ptrs[pid].store(new_ptr, std::memory_order_release);
//
//  The ME will seamlessly see the new partition on its next access without
//  any blocking or synchronization.
//
// -----------------------------------------------------------------------------
//  2. Deferred Reclamation (Grace Period)
// -----------------------------------------------------------------------------
//
//  After the atomic swap, the old partition must NOT be immediately freed,
//  because the ME may still be using it.  To handle this safely:
//
//    • The worker pushes the old pointer into a “reclamation queue” with
//      a timestamp or epoch counter.
//    • Old partitions are freed only after a short grace period
//      (e.g., 10–50 µs), enough for any in-flight ME operation to finish.
//
//  This mirrors an RCU (Read-Copy-Update) grace period without requiring
//  a full RCU framework.
//
// -----------------------------------------------------------------------------
//  3. Background Worker Logic
// -----------------------------------------------------------------------------
//
//  The worker receives the latest traded price (last_price) from the ME via
//  a simple atomic variable or SPSC channel:
//
//      std::atomic<Price> last_price;
//
//  Because only the latest value matters (older prices are obsolete),
//  a single atomic field is sufficient—no need for a ring buffer or sequence.
//
//  The worker maintains a moving "heat window" of ±N partitions around
//  last_price.  On each tick:
//
//    • Identify which partitions have entered the window → promote().
//    • Identify which have left the window → demote().
//
//  Each promote/demote is performed via the versioned copy + atomic swap
//  protocol described above.
//
// -----------------------------------------------------------------------------
//  4. Hysteresis Control
// -----------------------------------------------------------------------------
//
//  To prevent thrashing (frequent promote/demote oscillations when the price
//  hovers near a boundary), the worker applies hysteresis:
//
//    • Two radii are defined:
//         HOT_RADIUS    → the base heat window (always kept hot)
//         COLD_MARGIN   → an extra buffer region
//
//    • A partition must move beyond HOT_RADIUS + COLD_MARGIN before being
//      demoted to cold.
//
//    • Similarly, a partition must move within HOT_RADIUS - COLD_MARGIN
//      before being promoted to hot.
//
//  This ensures smooth transitions even in highly volatile markets.
//
// -----------------------------------------------------------------------------
//  5. Safe Concurrent Access Summary
// -----------------------------------------------------------------------------
//
//   ME Thread:
//     - Reads atomic partition pointers on each lookup.
//     - Updates version counters on each structural modification.
//     - Never performs allocation, swap, or reclamation.
//
//   Worker Thread:
//     - Copies partitions after verifying version stability.
//     - Swaps pointers atomically.
//     - Defers freeing of old partitions (RCU-style grace).
//
//   Both:
//     - Operate independently and continuously.
//     - Never block each other.
//     - Maintain eventual consistency of the OrderBook structure.
//
// -----------------------------------------------------------------------------
//  6. Future Extensions
// -----------------------------------------------------------------------------
//
//    • ColdPartitionPool / ColdPriceLevelPool:
//        Preallocate all cold-side memory to eliminate small allocations
//        during promotion/demotion.  Cold partitions link ColdPriceLevels
//        via intrusive lists or index-based nodes.
//
//    • Copy-on-Write Partition Migration:
//        During long copy times, ME mutations can be logged as deltas and
//        replayed after copy completion to achieve fully atomic migration.
//
//    • Adaptive Reclamation Timing:
//        Use real latency histograms from ME metrics to tune grace periods.
//
// -----------------------------------------------------------------------------
//  7. Key Properties
// -----------------------------------------------------------------------------
//
//    • 100% Lock-Free for ME.
//    • Deterministic per-message latency.
//    • Bounded memory footprint.
//    • Full isolation between trading and memory management concerns.
//
// =============================================================================




// =============================================================================
//  FUTURE EXTENSION: PREALLOCATED POOLS FOR COLD PARTITIONS & PRICE LEVELS
// =============================================================================
//
//  Motivation
//  ----------
//  The cold tier (ColdPartition / ColdPriceLevel) is designed to handle orders
//  placed far from the current market price — “cold” price regions that are
//  rarely touched by real-time flow. Although these structures are not on the
//  ultra-low-latency path, they must still be fully deterministic and
//  allocation-free once initialized.
//
//  A naïve ColdPartition implementation uses dynamic containers
//  (e.g. std::vector<ColdPriceLevel>) or heap allocations when levels are
//  inserted. This creates latency outliers and memory fragmentation that are
//  unacceptable in a production exchange environment.
//
//  To guarantee bounded latency and constant memory footprint, we introduce
//  *preallocated object pools* for both ColdPartitions and ColdPriceLevels.
//
// -----------------------------------------------------------------------------
//  Overview
// -----------------------------------------------------------------------------
//
//   ┌───────────────────────┐       alloc/free       ┌──────────────────────┐
//   │  ColdPriceLevelPool   │  ------------------->  │   ColdPriceLevel[n]  │
//   │ (shared intrusive pool│                        │   contiguous storage │
//   │  of reusable nodes)   │  <-------------------  │   + free-list stack  │
//   └───────────────────────┘                        └──────────────────────┘
//
//   ┌───────────────────────┐       alloc/free       ┌──────────────────────┐
//   │   ColdPartitionPool   │  ------------------->  │   ColdPartition[m]   │
//   │ (manages fixed slots  │                        │   each referencing   │
//   │  of sparse partitions)│  <-------------------  │   ColdPriceLevelPool │
//   └───────────────────────┘                        └──────────────────────┘
//
//  Each pool is created once at engine startup and provides O(1) allocation
//  and release through an internal free-list of indices. No system calls or
//  heap allocations occur after initialization.
//
// -----------------------------------------------------------------------------
//  ColdPriceLevelPool
// -----------------------------------------------------------------------------
//
//  - Holds an array of ColdPriceLevel objects (typically 50K–500K capacity).
//  - Each node can represent one active price level in a cold partition.
//  - Provides lock-free or single-threaded O(1) allocation via index stack.
//  - Supports reset() to clear a node without freeing memory.
//  - Returned indices are reused deterministically (no heap reuse patterns).
//
//  struct ColdPriceLevel {
//      PriceLevel core;       // full PriceLevel info (reused)
//      uint32_t next_idx;     // next node index in pool
//      uint32_t prev_idx;     // previous node index
//  };
//
//  Allocation Pattern:
//      uint32_t idx;
//      ColdPriceLevel* lvl = pool.allocate(idx);
//      lvl->core.set_price(p);
//      ...
//      pool.release(idx);
//
//  This pool allows all cold partitions to share a common memory arena.
//  Memory usage = O(capacity) and fixed at startup.
//
// -----------------------------------------------------------------------------
//  ColdPartitionPool
// -----------------------------------------------------------------------------
//
//  - Manages a preallocated array of ColdPartition structures.
//  - Each partition references ColdPriceLevelPool for its level storage.
//  - Provides allocate() and release() methods with O(1) free-list semantics.
//  - May later be extended with metrics (occupancy ratio, usage histograms).
//
//  struct ColdPartition {
//      PartitionId partid;
//      uint32_t head_idx;
//      uint32_t tail_idx;
//      uint32_t level_count;
//  };
//
//  When a cold partition is promoted to a hot partition, its slot can be
//  recycled by returning it to the pool, ensuring bounded total memory.
//
// -----------------------------------------------------------------------------
//  Access Pattern
// -----------------------------------------------------------------------------
//
//   1. The Matching Engine inserts an order far from the market.
//   2. PriceLevelStore determines that the corresponding partition is cold.
//   3. If the partition doesn’t exist, it calls:
//          ColdPartition* cp = cold_partition_pool.allocate(partid);
//   4. Each ColdPriceLevel within the partition is obtained from the shared
//      ColdPriceLevelPool via allocate().
//   5. When the partition becomes empty or is promoted, its levels and the
//      partition itself are released back into their respective pools.
//
// -----------------------------------------------------------------------------
//  Threading Model
// -----------------------------------------------------------------------------
//
//  - The cold pools are typically owned by the same thread/core that runs the
//    orchestrator or the PriceLevelStore for that instrument.
//  - Since cold operations are rare, a simple single-threaded free-list is
//    adequate; lock-free queues can be added later if multiple threads access.
//
// -----------------------------------------------------------------------------
//  Advantages
// -----------------------------------------------------------------------------
//
//  - Zero dynamic allocations during runtime.
//  - Fully deterministic latency, even for rare cold operations.
//  - Predictable memory usage across thousands of instruments.
//  - High locality (all cold data resides in contiguous arrays).
//  - Fast promotion/demotion: cold <-> hot conversion is just a data copy.
//
// -----------------------------------------------------------------------------
//  Future Enhancements
// -----------------------------------------------------------------------------
//
//  - Lock-free free-lists (if cross-thread cold handling is needed).
//  - NUMA-aware sub-pools for multi-socket deployments.
//  - Instrument-scoped pool partitions to reduce cache contention.
//  - Integration with metrics for live memory telemetry.
//
// =============================================================================
