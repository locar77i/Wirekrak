#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <vector>
#include <memory>
#include <windows.h>

#include "lcr/lockfree/spsc_queue.hpp"
#include "lcr/lockfree/spsc_ring.hpp"

using namespace std::chrono;

struct Msg {
    uint64_t value;
    //uint64_t data[3];
};

constexpr size_t N = 1 << 16;
constexpr int DURATION_SEC = 5;
constexpr size_t BATCH_SIZE = 128;

void pin_thread(int core) {
    SetThreadAffinityMask(GetCurrentThread(), 1ull << core);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

struct Result {
    std::string name;
    double throughput_mps;
    uint64_t total_ops;
};

// ------------------------------------------------------------
// 1. Standard Queue Benchmark (Push/Pop)
// ------------------------------------------------------------
Result run_queue() {
    auto queue_ptr = std::make_unique<lcr::lockfree::spsc_queue<Msg, N>>();
    auto& queue = *queue_ptr; // important: avoid smart pointer in hot loop

    std::atomic<bool> start{false}, stop{false};
    std::atomic<uint64_t> consumed{0};

    std::thread producer([&] {
        pin_thread(0);
        while (!start.load(std::memory_order_acquire));

        uint64_t counter = 0;
        Msg msg{};

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
                auto v = msg.value; (void)v;  // prevent optimization
                local++;
            }
        }
        consumed.store(local, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SEC));
    stop.store(true, std::memory_order_release);
    producer.join(); consumer.join();
    auto t1 = high_resolution_clock::now();

    double secs = duration<double>(t1 - t0).count();
    return {"Standard Queue", (consumed.load() / secs) / 1e6, consumed.load()};
}

// ------------------------------------------------------------
// 2. Standard Ring Benchmark (Two-Phase Single)
// ------------------------------------------------------------
Result run_ring_single() {
    auto ring_ptr = std::make_unique<lcr::lockfree::spsc_ring<Msg, N>>();
    auto& ring = *ring_ptr; // important: avoid smart pointer in hot loop

    std::atomic<bool> start{false}, stop{false};
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
                auto v = slot->value; (void)v;  // prevent optimization
                ring.release_consumer_slot();
                local++;
            }
        }
        consumed.store(local, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SEC));
    stop.store(true, std::memory_order_release);
    producer.join(); consumer.join();
    auto t1 = high_resolution_clock::now();

    double secs = duration<double>(t1 - t0).count();
    return {"Ring Zero-Copy (Single)", (consumed.load() / secs) / 1e6, consumed.load()};
}

// ------------------------------------------------------------
// 3. Ring BATCH Benchmark (Ultra-High Throughput)
// ------------------------------------------------------------
Result run_ring_batch() {
    auto ring_ptr = std::make_unique<lcr::lockfree::spsc_ring<Msg, N>>();
    auto& ring = *ring_ptr; // avoid smart pointer in hot loop

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
                    Msg* slot = ring.producer_slot(head, i);
                    slot->value = counter++;
                }
                ring.commit_producer_batch(BATCH_SIZE);
            }
        }
    });

    std::thread consumer([&] {
        pin_thread(1);
        while (!start.load(std::memory_order_acquire));

        uint64_t local = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            size_t tail;

            if (ring.try_acquire_consumer_batch(BATCH_SIZE, tail)) {
                for (size_t i = 0; i < BATCH_SIZE; ++i) {
                    Msg* slot = ring.consumer_slot(tail, i);
                    auto v = slot->value; (void)v;  // prevent optimization
                    local++;
                }
                ring.release_consumer_batch(BATCH_SIZE);
            }
        }

        consumed.store(local, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SEC));

    stop.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    auto t1 = high_resolution_clock::now();

    double secs = duration<double>(t1 - t0).count();

    return {
        "Ring Zero-Copy (BATCH)",
        (consumed.load() / secs) / 1e6,
        consumed.load()
    };
}

int main() {
    std::cout << "Starting Ultra-Low Latency SPSC Benchmarks...\n";
    std::cout << "Message Size: " << sizeof(Msg) << " bytes\n";
    std::cout << "Capacity:     " << N << " slots\n";
    std::cout << "---------------------------------------------------------\n";

    auto r1 = run_queue();
    std::cout << r1.name << ": " << r1.throughput_mps << " M msg/s\n";

    auto r2 = run_ring_single();
    std::cout << r2.name << ": " << r2.throughput_mps << " M msg/s\n";

    auto r3 = run_ring_batch();
    std::cout << r3.name << ": " << r3.throughput_mps << " M msg/s\n";

    std::cout << "---------------------------------------------------------\n";
    double boost = (r3.throughput_mps / r1.throughput_mps);
    std::cout << "Batching Advantage vs Queue: " << boost << "x faster\n";

    return 0;
}
