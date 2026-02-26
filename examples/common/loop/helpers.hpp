#pragma once

#include <thread>

#include "wirekrak/core.hpp"


namespace wirekrak::core::loop {

// Manages idle spins to avoid busy-waiting.
// If no work was done, it increments idle_spins. Once idle_spins exceeds max_idle_spins, it yields and resets the counter.
// Example usage:
// int idle_spins = 0;
// while (running && session.is_active()) {
//     bool did_work = false;
//     session.poll();
//     // ... process messages ...
//     manage_idle_spins(did_work, idle_spins);
// }
inline void manage_idle_spins(bool& did_work, int& idle_spins, int max_idle_spins = 100) {
    if (did_work) {
        idle_spins = 0;
        did_work = false;
    } else {
        if (++idle_spins > max_idle_spins) {
            std::this_thread::yield();
            idle_spins = 0;
        }
    }
}



// -----------------------------------------------------------------------------
// Helper to drain all available messages
// -----------------------------------------------------------------------------
template<typename Session>
inline bool drain_messages(Session& session) {
    using namespace wirekrak::core::protocol::kraken::schema;

    bool did_work = false;

    // Observe latest pong (liveness signal)
    system::Pong last_pong;
    if (session.try_load_pong(last_pong)) {
        did_work = true;
    }

    // Observe latest status
    status::Update last_status;
    if (session.try_load_status(last_status)) {
        did_work = true;
    }

    // Drain protocol errors (required)
    session.drain_rejection_messages([&](const rejection::Notice&) {
        did_work = true;
    });

    // Drain data-plane trade messages (required)
    session.drain_trade_messages([&](const trade::Response&) {
        did_work = true;
    });

    // Drain data-plane book messages (required)
    session.drain_book_messages([&](const book::Response&) {
        did_work = true;
    });

    return did_work;
}

// -----------------------------------------------------------------------------
// Helper to drain all available messages
// -----------------------------------------------------------------------------
template<typename Session>
inline bool drain_and_print_messages(Session& session) {
    using namespace wirekrak::core::protocol::kraken::schema;

    bool did_work = false;

    // Observe latest pong (liveness signal)
    system::Pong last_pong;
    if (session.try_load_pong(last_pong)) {
        std::cout << " -> " << last_pong << std::endl;
        did_work = true;
    }

    // Observe latest status
    status::Update last_status;
    if (session.try_load_status(last_status)) {
        std::cout << " -> " << last_status << std::endl;
        did_work = true;
    }

    // Drain protocol errors (required)
    session.drain_rejection_messages([&](const rejection::Notice& msg) {
        std::cout << " -> " << msg << std::endl;
        did_work = true;
    });

    // Drain data-plane trade messages (required)
    session.drain_trade_messages([&](const trade::Response& msg) {
        std::cout << " -> " << msg << std::endl;
        did_work = true;
    });

    // Drain data-plane book messages (required)
    session.drain_book_messages([&](const book::Response& msg) {
        std::cout << " -> " << msg << std::endl;
        did_work = true;
    });

    return did_work;
}

} // namespace wirekrak::core::loop
