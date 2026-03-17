#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <windows.h>

#include "lcr/lockfree/spsc_queue.hpp"
#include "lcr/lockfree/spsc_ring.hpp"

using namespace std::chrono;

struct Msg {
    uint64_t value;
};

constexpr size_t N = 1 << 16;
constexpr int DURATION_SEC = 10;

// ------------------------------------------------------------
// Thread pinning
// ------------------------------------------------------------
void pin_thread(int core) {
    SetThreadAffinityMask(GetCurrentThread(), 1ull << core);
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
    lcr::lockfree::spsc_queue<Msg, N> queue;

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> consumed{0};

    std::thread producer([&] {
        pin_thread(0);
        while (!start.load(std::memory_order_acquire));

        uint64_t counter = 0;
        Msg msg;

        while (!stop.load(std::memory_order_relaxed)) {
            msg.value = counter;
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
                (void)msg.value;
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
    lcr::lockfree::spsc_ring<Msg, N> ring;

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> consumed{0};

    std::thread producer([&] {
        pin_thread(0);
        while (!start.load(std::memory_order_acquire));

        uint64_t counter = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            if (auto* slot = ring.acquire_producer_slot()) {
                slot->value = counter++;
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
                (void)slot->value;
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
    std::cout << "Ring advantage   : +" << diff << " %\n";

    return 0;
}
