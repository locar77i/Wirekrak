#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <vector>
#include <cassert>
#include <windows.h>

#include "lcr/lockfree/spsc_ring.hpp"


using namespace std::chrono;

struct Msg {
    uint64_t value;
};

constexpr size_t N = 1 << 16;

lcr::lockfree::spsc_ring<Msg, N> ring;

std::atomic<bool> start{false};
std::atomic<bool> stop{false};
std::atomic<uint64_t> produced{0};
std::atomic<uint64_t> consumed{0};

void pin_thread(int core) {
    SetThreadAffinityMask(GetCurrentThread(), 1ull << core);
}

void producer() {
    pin_thread(0);

    while (!start.load(std::memory_order_acquire));

    uint64_t local_counter = 0; // Contador local (sin overhead atómico)

    while (!stop.load(std::memory_order_relaxed)) {
        if (auto* slot = ring.acquire_producer_slot()) {
            slot->value = local_counter++;
            ring.commit_producer_slot();
        }
    }
    produced.store(local_counter, std::memory_order_relaxed); // Actualiza el contador global al final
}

void consumer() {
    pin_thread(1);

    while (!start.load(std::memory_order_acquire));

    uint64_t local_consumed = 0; // Contador local

    while (!stop.load(std::memory_order_relaxed)) {
        if (auto* slot = ring.peek_consumer_slot()) {
            auto v = slot->value;
            (void)v;
            ring.release_consumer_slot();
            local_consumed++;
        }
    }

    consumed.store(local_consumed, std::memory_order_relaxed); // Actualiza el contador global al final
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);

    std::this_thread::sleep_for(std::chrono::seconds(1)); // warmup

    auto t0 = high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::seconds(10));

    stop.store(true, std::memory_order_release);

    t1.join();
    t2.join();

    auto t1_end = high_resolution_clock::now();
    double seconds = duration<double>(t1_end - t0).count();

    uint64_t ops = consumed.load();

    std::cout << "Throughput: " << (ops / seconds) / 1e6 << " M msg/s\n";
}
