//------------------------------------------------------------------------------
// Managed SPSC Ring Benchmark (Local / Optimized)
//
// Measures best-case throughput with full compiler visibility.
//
// Characteristics:
//   • Local ring + pool (owned via unique_ptr)
//   • Producer/consumer lambdas
//   • Minimal workload (no promotion)
//   • Maximizes inlining + optimization
//
//------------------------------------------------------------------------------

#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <cstdint>

#include <windows.h>

#include "lcr/buffer/managed_spsc_ring.hpp"
#include "lcr/buffer/managed_slot.hpp"
#include "lcr/memory/block_pool.hpp"
#include "lcr/metrics/latency_histogram.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/format.hpp"

using namespace std::chrono;

//------------------------------------------------------------------------------
// Config
//------------------------------------------------------------------------------

constexpr size_t N = 1 << 16;
constexpr size_t POOL_BLOCK_SIZE = 4096;
constexpr size_t POOL_BLOCK_COUNT = 1 << 14;

//------------------------------------------------------------------------------
// Thread pinning
//------------------------------------------------------------------------------

void pin_thread(int core) {
    SetThreadAffinityMask(GetCurrentThread(), 1ull << core);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

//------------------------------------------------------------------------------
// Main benchmark
//------------------------------------------------------------------------------

int main() {
    auto& clock = lcr::system::monotonic_clock::instance();

    using pool_t = lcr::memory::block_pool;
    using slot_t = lcr::buffer::managed_slot<8>;
    using ring_t = lcr::buffer::managed_spsc_ring<slot_t, pool_t, N>;

    // Local ownership (important for compiler visibility)
    auto pool_ptr = std::make_unique<pool_t>(POOL_BLOCK_SIZE, POOL_BLOCK_COUNT);
    auto ring_ptr = std::make_unique<ring_t>(*pool_ptr);
    // References for hot loop
    auto& pool = *pool_ptr;
    auto& ring = *ring_ptr;

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> consumed{0};

    lcr::metrics::latency_histogram handoff_latency;

    std::cout << "---------------------------------------------------------\n";
    std::cout << "Managed SPSC Ring Benchmark (minimal+timestamp)\n";
    std::cout << "Slot Size      : " << sizeof(slot_t) << " bytes\n";
    std::cout << "Ring Capacity  : " << ring.capacity() << " slots\n";
    std::cout << "Pool Capacity  : " << pool.capacity() << " blocks\n";

    //--------------------------------------------------------------------------
    // Producer
    //--------------------------------------------------------------------------

    std::thread producer([&] {
        pin_thread(0);  // Optional: pin producer to core 0 for more stable measurements

        while (!start.load(std::memory_order_acquire));

        uint64_t counter = 0;

        while (!stop.load(std::memory_order_relaxed)) {

            auto* slot = ring.acquire_producer_slot();
            if (!slot) [[unlikely]] continue;

/*
            auto r = ring.reserve(slot, 1);
            // Optional sanity (should always be None)
            if (r != lcr::buffer::PromotionResult::None) [[unlikely]] {
                ring.discard_producer_slot(slot);
                continue;
            }
*/

            // Minimal write with dependency (and sampled timestamp)
            if ((counter & 0x3FF) == 0) [[unlikely]] {
                slot->set_timestamp(clock.now_ns());
                slot->write_ptr()[0] = 1; // mark sampled
            } else {
                slot->write_ptr()[0] = 0;
            }
            slot->commit(1);

            ring.commit_producer_slot();
            ++counter;
        }
    });

    //--------------------------------------------------------------------------
    // Consumer
    //--------------------------------------------------------------------------

    std::thread consumer([&] {
        pin_thread(1);  // Optional: pin consumer to core 1 for more stable measurements

        while (!start.load(std::memory_order_acquire));

        uint64_t local = 0;

        while (!stop.load(std::memory_order_relaxed)) {

            auto* slot = ring.peek_consumer_slot();
            if (!slot) [[unlikely]] continue;

            auto v = slot->data()[0];
            if (v == 1) [[unlikely]] {
                handoff_latency.record(slot->timestamp(), clock.now_ns());
            }

            ring.release_consumer_slot(slot);
            local++;
        }

        consumed.store(local, std::memory_order_relaxed);
    });

    //--------------------------------------------------------------------------
    // Run
    //--------------------------------------------------------------------------

    std::this_thread::sleep_for(std::chrono::seconds(1)); // warmup

    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::seconds(10));

    stop.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    auto t1 = high_resolution_clock::now();
    double seconds = duration<double>(t1 - t0).count();

    uint64_t ops = consumed.load(std::memory_order_relaxed);

    std::cout << "----------------------------------------\n";
    std::cout << "Throughput: " << lcr::format_throughput(ops / seconds) << "\n";
    std::cout << "Consumed  : " << lcr::format_number_exact(ops) << "\n";
    std::cout << "Latency   : "; handoff_latency.dump(std::cout); std::cout << "\n";
    std::cout << "----------------------------------------\n";

    return 0;
}
