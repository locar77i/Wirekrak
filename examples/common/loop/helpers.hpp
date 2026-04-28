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
inline void manage_idle_spins(bool& did_work, int& idle_spins, int max_idle_spins = 1024) {
    if (did_work) {
        idle_spins = 0;
        did_work = false;
        return;
    }

    ++idle_spins;

    if (idle_spins < 64) {
        _mm_pause();                               // 1x
    }
    else if (idle_spins < 256) {
        for (int i = 0; i < 8; ++i) _mm_pause();   // 8x
    }
    else if (idle_spins < max_idle_spins) {
        for (int i = 0; i < 32; ++i) _mm_pause();  // 32x 
    }
    else {
        for (int i = 0; i < 128; ++i) _mm_pause(); // 128x 
    }
}



// -----------------------------------------------------------------------------
// Helper to drain all available messages
// -----------------------------------------------------------------------------
template<typename Session>
inline bool drain_messages(Session& session) {
    using namespace wirekrak::core::protocol::kraken::schema;

    bool did_work = false;

    auto drained = session.data_plane().drain_all(
        [&](auto&&) noexcept {
            did_work = true;
        }
    );

    return did_work;
}

// -----------------------------------------------------------------------------
// Helper to drain all available messages (DataPlane version)
// -----------------------------------------------------------------------------
template<typename Session>
inline bool drain_and_print_messages(Session& session) {
    using namespace wirekrak::core::protocol::kraken::schema;

    bool did_work = false;

    auto& dp = session.data_plane();

    // -------------------------------------------------------------------------
    // States (latest values)
    // -------------------------------------------------------------------------

    if (const auto* pong = dp.template get<system::Pong>()) {
        std::cout << " -> " << *pong << std::endl;
        did_work = true;
    }

    if (const auto* status = dp.template get<status::Update>()) {
        std::cout << " -> " << *status << std::endl;
        did_work = true;
    }

    // -------------------------------------------------------------------------
    // Messages (streams)
    // -------------------------------------------------------------------------

    dp.template drain<rejection::Notice>([&](const auto& msg) {
        std::cout << " -> " << msg << std::endl;
        did_work = true;
    });

    dp.template drain<trade::Response>([&](const auto& msg) {
        std::cout << " -> " << msg << std::endl;
        did_work = true;
    });

    dp.template drain<book::Response>([&](const auto& msg) {
        std::cout << " -> " << msg << std::endl;
        did_work = true;
    });

    return did_work;
}

} // namespace wirekrak::core::loop
