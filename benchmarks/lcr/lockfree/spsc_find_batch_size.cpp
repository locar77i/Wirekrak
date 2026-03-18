#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>
#include <windows.h>

#include "lcr/lockfree/spsc_ring.hpp"

using namespace std::chrono;

struct Msg {
    uint64_t value;
};

constexpr size_t N = 1 << 16;
constexpr int DURATION_SEC = 5;

// ------------------------------------------------------------
// Thread pinning
// ------------------------------------------------------------
void pin_thread(int core) {
    SetThreadAffinityMask(GetCurrentThread(), 1ull << core);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

// ------------------------------------------------------------
// Result
// ------------------------------------------------------------
struct Result {
    size_t batch;
    double throughput_mps;
};

// ------------------------------------------------------------
// Benchmark for a given batch size
// ------------------------------------------------------------
Result run_batch(size_t BATCH_SIZE) {
    auto ring_ptr = std::make_unique<lcr::lockfree::spsc_ring<Msg, N>>();
    auto& ring = *ring_ptr;

    std::atomic<bool> start{false}, stop{false};
    std::atomic<uint64_t> consumed{0};

    std::thread producer([&] {
        pin_thread(0);
        while (!start.load(std::memory_order_acquire));

        uint64_t counter = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            size_t head;
            if (ring.try_acquire_producer_batch(BATCH_SIZE, head)) {
                for (size_t i = 0; i < BATCH_SIZE; ++i) {
                    ring.producer_slot(head, i)->value = counter++;
                }
                ring.commit_producer_batch(BATCH_SIZE);
            }
        }
    });

    std::thread consumer([&] {
        pin_thread(1);
        while (!start.load(std::memory_order_acquire));

        uint64_t local = 0;
        uint64_t sink = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            size_t tail;
            if (ring.try_acquire_consumer_batch(BATCH_SIZE, tail)) {
                for (size_t i = 0; i < BATCH_SIZE; ++i) {
                    sink = ring.consumer_slot(tail, i)->value;
                    (void)sink;
                    local++;
                }
                ring.release_consumer_batch(BATCH_SIZE);
            }
        }

        consumed.store(local, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SEC));

    stop.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    auto t1 = high_resolution_clock::now();

    double secs = duration<double>(t1 - t0).count();

    return { BATCH_SIZE, (consumed.load() / secs) / 1e6 };
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main() {
    std::vector<size_t> batches = {
        1, 2, 4, 8, 16, 32, 64, 128, 256
    };

    std::cout << "SPSC Ring Batch Sweep (" << DURATION_SEC << "s)\n\n";
    std::cout << "Batch\tThroughput (M msg/s)\n";
    std::cout << "---------------------------------\n";

    double best = 0.0;
    size_t best_batch = 0;

    for (auto b : batches) {
        auto res = run_batch(b);

        std::cout << res.batch << "\t" << res.throughput_mps << "\n";

        if (res.throughput_mps > best) {
            best = res.throughput_mps;
            best_batch = res.batch;
        }
    }

    std::cout << "\nBest batch size: " << best_batch << " -> " << best << " M msg/s\n";

    return 0;
}
