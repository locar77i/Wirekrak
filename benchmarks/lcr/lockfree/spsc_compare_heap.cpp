//------------------------------------------------------------------------------
// SPSC Queue vs Ring Throughput Benchmark
//------------------------------------------------------------------------------
//
// This benchmark measures and compares the throughput of two SPSC variants:
//
//   • spsc_queue  → push/pop API (value semantics)
//   • spsc_ring   → two-phase API (zero-copy semantics)
//
// Both implementations are executed under identical conditions:
//   • Dedicated producer and consumer threads
//   • CPU core pinning to avoid migration
//   • Fixed-duration steady-state measurement
//
// IMPORTANT NOTE ON BENCHMARK STRUCTURE
//
// This benchmark intentionally uses:
//   • Local (stack-allocated) queue/ring instances
//   • Lambda-based producer/consumer threads
//
// This design is critical to obtain representative peak performance.
//
// Why?
//
// 1. Local ownership (non-global objects)
//    - Enables stronger compiler alias analysis
//    - Guarantees no external visibility or interference
//    - Allows more aggressive inlining and optimization
//
// 2. Lambda-based threads
//    - Improve compiler visibility of the full execution context
//    - Reduce function call boundaries
//    - Enable tighter hot-loop optimization
//
// 3. Combined execution context
//    - Warms up CPU frequency (turbo boost)
//    - Stabilizes scheduling and cache behavior
//
// As a result, this benchmark typically produces significantly higher
// throughput than standalone/global-state benchmarks.
//
// Interpretation guideline:
//
//   • For small trivial types (e.g., uint64_t):
//       The compiler may optimize queue copies into direct stores,
//       making spsc_queue approach zero-copy performance.
//
//   • For larger or non-trivial payloads:
//       spsc_ring (zero-copy) is expected to outperform spsc_queue.
//
// This benchmark is designed to reflect best-case, production-like
// conditions where:
//   • Objects are locally owned
//   • Code is fully optimized
//   • Hot paths are free of unnecessary abstraction barriers
//
//------------------------------------------------------------------------------

#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <windows.h>

#include "lcr/lockfree/spsc_queue.hpp"
#include "lcr/lockfree/spsc_ring.hpp"

using namespace std::chrono;

struct Msg {
    uint64_t value[40]; // 320 bytes total, larger than typical cache line to amplify copy cost
};

constexpr size_t N = 1 << 16;
constexpr int DURATION_SEC = 10;

// ------------------------------------------------------------
// Thread pinning
// ------------------------------------------------------------
void pin_thread(int core) {
    SetThreadAffinityMask(GetCurrentThread(), 1ull << core);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

// ------------------------------------------------------------
// Benchmark result
// ------------------------------------------------------------
struct Result {
    double throughput_mps;
    uint64_t ops;
};

// ------------------------------------------------------------
// Queue benchmark
// ------------------------------------------------------------
Result run_queue() {
    auto queue_ptr = std::make_unique<lcr::lockfree::spsc_queue<Msg, N>>();
    auto& queue = *queue_ptr; // important: avoid smart pointer in hot loop

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> consumed{0};

    std::thread producer([&] {
        pin_thread(0);
        while (!start.load(std::memory_order_acquire));

        uint64_t counter = 0;
        Msg msg;

        while (!stop.load(std::memory_order_relaxed)) {
            //msg.value = counter;
            if (queue.push(msg)) {
                counter++;
            }
        }
    });

    std::thread consumer([&] {
        pin_thread(1);
        while (!start.load(std::memory_order_acquire));

        uint64_t local = 0;
        Msg msg;

        while (!stop.load(std::memory_order_relaxed)) {
            if (queue.pop(msg)) {
                //(void)msg.value;
                local++;
            }
        }

        consumed.store(local, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::seconds(1)); // warmup

    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SEC));

    stop.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    auto t1 = high_resolution_clock::now();

    double seconds = duration<double>(t1 - t0).count();
    uint64_t ops = consumed.load();

    return { (ops / seconds) / 1e6, ops };
}

// ------------------------------------------------------------
// Ring benchmark
// ------------------------------------------------------------
Result run_ring() {
    auto ring_ptr = std::make_unique<lcr::lockfree::spsc_ring<Msg, N>>();
    auto& ring = *ring_ptr; // important: avoid smart pointer in hot loop

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> consumed{0};

    std::thread producer([&] {
        pin_thread(0);
        while (!start.load(std::memory_order_acquire));

        uint64_t counter = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            if (auto* slot = ring.acquire_producer_slot()) {
                //slot->value = counter++;
                ring.commit_producer_slot();
            }
        }
    });

    std::thread consumer([&] {
        pin_thread(1);
        while (!start.load(std::memory_order_acquire));

        uint64_t local = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            if (auto* slot = ring.peek_consumer_slot()) {
                //(void)slot->value;
                ring.release_consumer_slot();
                local++;
            }
        }

        consumed.store(local, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::seconds(1)); // warmup

    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SEC));

    stop.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    auto t1 = high_resolution_clock::now();

    double seconds = duration<double>(t1 - t0).count();
    uint64_t ops = consumed.load();

    return { (ops / seconds) / 1e6, ops };
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main() {
    std::cout << "Running SPSC benchmarks (" << DURATION_SEC << "s each)...\n\n";

    auto queue_res = run_queue();
    auto ring_res  = run_ring();

    double diff = ((ring_res.throughput_mps - queue_res.throughput_mps)
                  / queue_res.throughput_mps) * 100.0;

    std::cout << "Results:\n";
    std::cout << "---------------------------------\n";
    std::cout << "Queue throughput : " << queue_res.throughput_mps << " M msg/s\n";
    std::cout << "Ring throughput  : " << ring_res.throughput_mps  << " M msg/s\n";
    std::cout << "---------------------------------\n";
    std::cout << "Ring advantage   : " << diff << " %\n";

    return 0;
}
