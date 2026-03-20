//------------------------------------------------------------------------------
// Managed SPSC Ring Benchmark (Realistic / Local Optimized)
//
// Same workload as baseline, but:
//   • Local ring + pool (unique_ptr)
//   • Lambda producer/consumer
//   • Maximum compiler visibility
//
//------------------------------------------------------------------------------

#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <cstdint>
#include <cstring>

#include "lcr/buffer/managed_spsc_ring.hpp"
#include "lcr/buffer/managed_slot.hpp"
#include "lcr/memory/block_pool.hpp"
#include "lcr/metrics/latency_histogram.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/system/thread_affinity.hpp"
#include "lcr/format.hpp"

using namespace std::chrono;

//------------------------------------------------------------------------------
// Config
//------------------------------------------------------------------------------

constexpr size_t N = 1 << 8;
constexpr size_t POOL_BLOCK_SIZE = 8192;
constexpr size_t POOL_BLOCK_COUNT = 1 << 4;


//------------------------------------------------------------------------------
// Deterministic size distribution
//------------------------------------------------------------------------------

inline size_t size_dist(uint64_t i) {
    uint64_t x = i & 0xFF;

    if (x < 200) return 232;      // ~78%
    if (x < 252) return 1000;     // ~19%
    if (x < 253) return 2024;     // ~1%
    if (x < 254) return 4072;     // ~1%
    return 8192;                  // ~1%
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

int main() {
    auto& clock = lcr::system::monotonic_clock::instance();

    using slot_t = lcr::buffer::managed_slot<1000>;
    using pool_t = lcr::memory::block_pool;
    using ring_t = lcr::buffer::managed_spsc_ring<slot_t, pool_t, N>;

    // Local ownership (critical for optimization)
    auto pool_ptr = std::make_unique<pool_t>(POOL_BLOCK_SIZE, POOL_BLOCK_COUNT);
    auto ring_ptr = std::make_unique<ring_t>(*pool_ptr);

    auto& pool = *pool_ptr;
    auto& ring = *ring_ptr;

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};

    std::atomic<uint64_t> produced{0};
    std::atomic<uint64_t> consumed{0};
    std::atomic<uint64_t> promotions{0};
    std::atomic<uint64_t> pool_exhausted{0};

    lcr::metrics::latency_histogram handoff_latency;

    std::cout << "---------------------------------------------------------\n";
    std::cout << "Managed SPSC Ring Benchmark (realistic / optimized)\n";
    std::cout << "Slot Size      : " << sizeof(slot_t) << " bytes\n";
    std::cout << "Ring Capacity  : " << ring.capacity() << " slots\n";
    std::cout << "Pool Capacity  : " << pool.capacity() << " blocks\n";


    // ------------------------------------------------------------------------------
    // Simulated work functions to create more realistic processing time
    // ------------------------------------------------------------------------------

    auto do_producer_work = [](uint64_t& counter) {
        volatile uint64_t data;
        constexpr int N = 1000;
        for (int i = 0; i < N; ++i) { // Simulate logic/parsing overhead -> ~(N*3) nanoseconds
            data += (i * counter);
        }
    };

    auto do_consumer_work = [](volatile uint64_t& data, uint64_t& local) {
        constexpr int N = 1000;
        for (int i = 0; i < N; ++i) { // Simulate logic/parsing overhead -> ~(N*3) nanoseconds
            data += (i * local);
        }
    };
    
    //----------------------------------------------------------------------
    // Producer
    //----------------------------------------------------------------------

    std::thread producer([&] {
        lcr::system::pin_thread(0);

        while (!start.load(std::memory_order_acquire));

        uint64_t counter = 0;
        uint64_t local_promotions = 0;
        uint64_t local_pool_exhausted = 0;

        while (!stop.load(std::memory_order_acquire)) {

            auto* slot = ring.acquire_producer_slot();
            if (!slot) [[unlikely]] {
                continue;
            }

            size_t len = size_dist(counter);

            auto r = ring.reserve(slot, len);

            if (r == lcr::buffer::PromotionResult::PoolExhausted) [[unlikely]] {
                ring.discard_producer_slot(slot);
                local_pool_exhausted++;
                continue;
            }

            if (r == lcr::buffer::PromotionResult::TooLarge) [[unlikely]] {
                ring.discard_producer_slot(slot);
                continue;
            }

            if (r == lcr::buffer::PromotionResult::Success) {
                local_promotions++;
            }

            do_producer_work(counter);

            std::memset(slot->write_ptr(), 0xAB, len);   // Full write for realistic
            //slot->write_ptr()[0] = 0xAB;               // Minimal write for maximum throughput
            if ((counter & 0x3FF) == 0) [[unlikely]] {
                slot->set_timestamp(clock.now_ns());
                slot->write_ptr()[0] = 1; // mark sampled
            }
            slot->commit(len);

            ring.commit_producer_slot();
            counter++;
        }

        produced.store(counter, std::memory_order_relaxed);
        promotions.store(local_promotions, std::memory_order_relaxed);
        pool_exhausted.store(local_pool_exhausted, std::memory_order_relaxed);
    });

    //----------------------------------------------------------------------
    // Consumer
    //----------------------------------------------------------------------

    std::thread consumer([&] {
        lcr::system::pin_thread(1);

        while (!start.load(std::memory_order_acquire));

        uint64_t local = 0;
        volatile uint64_t data = 0; // Prevent optimization

        while (!stop.load(std::memory_order_acquire)) {

            auto* slot = ring.peek_consumer_slot();
            if (!slot) [[unlikely]] {
                continue;
            }

            data = slot->data()[0];
            if (data == 1) [[unlikely]] {
                handoff_latency.record(slot->timestamp(), clock.now_ns());
            }

            do_consumer_work(data, local);

            ring.release_consumer_slot(slot);
            local++;
        }

        consumed.store(local, std::memory_order_relaxed);
    });

    //----------------------------------------------------------------------
    // Run
    //----------------------------------------------------------------------

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::seconds(10));

    stop.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    auto t1 = high_resolution_clock::now();
    double seconds = duration<double>(t1 - t0).count();

    uint64_t ops = consumed.load(std::memory_order_relaxed);
    uint64_t prod = produced.load(std::memory_order_relaxed);
    uint64_t prom = promotions.load(std::memory_order_relaxed);
    uint64_t exhausted = pool_exhausted.load(std::memory_order_relaxed);

    std::cout << "----------------------------------------\n";
    std::cout << "Throughput: " << lcr::format_throughput(ops / seconds) << "\n";
    std::cout << "Produced:   " << lcr::format_number_exact(prod) << "\n";
    std::cout << "Consumed:   " << lcr::format_number_exact(ops) << "\n";
    std::cout << "Promotions: " << lcr::format_number_exact(prom)
              << " (" << (100.0 * prom / (double)prod) << "%)\n";
    std::cout << "Pool Exhausted: " << lcr::format_number_exact(exhausted) << "\n";
    std::cout << "Latency   : "; handoff_latency.dump(std::cout); std::cout << "\n";
    std::cout << "----------------------------------------\n";

    return 0;
}
