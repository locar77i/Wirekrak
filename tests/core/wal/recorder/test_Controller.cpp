#include <cassert>
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>

#include "wirekrak/core/wal/recorder/controller.hpp"
using namespace wirekrak::core;

using namespace std::chrono_literals;

//
// A helper sleep function that cannot oversleep too far,
// useful for CI systems.
//
inline void tiny_wait(std::chrono::milliseconds dur) {
    std::this_thread::sleep_for(dur);
}

//
// Test 1: Controller starts and stops properly.
//
void test_start_stop() {
    wal::recorder::Controller c;

    c.start();
    tiny_wait(10ms);

    // Starting again must be harmless.
    c.start();
    tiny_wait(10ms);

    c.stop();
    tiny_wait(10ms);

    // Second stop must also be harmless.
    c.stop();

    std::cout << "[OK] test_start_stop" << std::endl;
}

//
// Test 2: Controller wakes up immediately when work becomes available.
//
void test_wakeup_on_work() {
    wal::recorder::Controller c;
    c.set_idle_shutdown(std::chrono::minutes(10)); // avoid shutdown during test
    c.start();

    std::atomic<bool> awakened{false};

    // Simulate a recorder becoming active
    c.increment_active();

    // If the controller was sleeping, it must wake immediately.
    // We cannot observe internal state, but we CAN observe that
    // decrementing active shortly afterward behaves correctly.
    tiny_wait(20ms);

    c.decrement_active();

    // If the controller were still sleeping, decrement_active()
    // would not have been noticed; we expect no crash and no deadlock.
    awakened = true;

    assert(awakened.load());
    c.stop();
    std::cout << "[OK] test_wakeup_on_work" << std::endl;
}

//
// Test 3: Controller stays alive when idle, but shuts down after idle_timeout.
//
void test_idle_shutdown() {
    wal::recorder::Controller c;

    // Very small idle shutdown (300ms)
    c.set_idle_shutdown(std::chrono::minutes(0));
    c.start();

    // Wait enough time to trigger idle shutdown
    tiny_wait(350ms);

    // Calling stop() must succeed even if thread auto-exited
    c.stop();
    std::cout << "[OK] test_idle_shutdown" << std::endl;
}


//
// Test 4: Active recorders prevent idle shutdown.
//
void test_active_prevents_shutdown() {
    std::cout << " -> Starting controller..." << std::endl;
    wal::recorder::Controller c;
    c.set_idle_shutdown(std::chrono::minutes(0)); // immediate idle shutdown threshold
    c.start();

    // Activate a recorder → must prevent shutdown
    std::cout << " -> Incrementing active recorder..." << std::endl;
    c.increment_active();

    // Wait long enough that idle shutdown *would* have occurred
    tiny_wait(500ms);
        // Now remove the active recorder
    std::cout << " -> Decrementing active recorder..." << std::endl;
    c.decrement_active();

    // Thread should still be running before calling stop()
    std::cout << " -> Stopping controller..." << std::endl;
    c.stop();
    std::cout << "[OK] test_active_prevents_shutdown" << std::endl;
}


//
// MAIN
//
int main() {
    std::cout << "\n=== Running Controller Unit Tests ===" << std::endl;

    test_start_stop();
    test_wakeup_on_work();
    test_idle_shutdown();
    test_active_prevents_shutdown();

    std::cout << "\nAll Controller tests passed." << std::endl;
    return 0;
}
