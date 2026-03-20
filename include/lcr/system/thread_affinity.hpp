#pragma once

/*
===============================================================================
lcr::system::thread_affinity
===============================================================================

Cross-platform thread pinning and priority control utility for ultra-low latency
systems.

Design goals:
  • Zero-overhead abstraction (fully inlined)
  • No exceptions (ULL-safe)
  • Minimal, predictable behavior
  • Cross-platform (Windows + Linux)
  • No dynamic allocation
  • No hidden side effects

Why this exists:
  Thread scheduling jitter is one of the dominant sources of tail latency in
  ultra-low latency systems. Pinning threads to specific CPU cores ensures:

    - Stable cache locality (L1/L2 reuse)
    - Reduced context switching
    - Deterministic producer/consumer alignment
    - Lower latency variance (p99+ improvement)

This utility centralizes affinity + priority handling instead of duplicating
platform-specific code across benchmarks and production paths.

-------------------------------------------------------------------------------
Usage:
-------------------------------------------------------------------------------

  // Pin current thread to core 2 with high priority
  lcr::system::pin_thread(2);

  // Pin with explicit priority
  lcr::system::pin_thread(3, lcr::system::thread_priority::realtime);

  // Pin std::thread
  std::thread t(...);
  lcr::system::pin_thread(t, 1);

-------------------------------------------------------------------------------
Important notes (ULL context):
-------------------------------------------------------------------------------

  • Pinning alone is NOT sufficient for deterministic latency.
    For best results combine with:
      - CPU isolation (Linux: isolcpus / taskset)
      - Disabling frequency scaling / C-states
      - NUMA-aware allocation
      - Dedicated cores per role (producer / consumer)

  • Linux realtime priority requires CAP_SYS_NICE privileges.

  • Windows affinity API (SetThreadAffinityMask) is limited to 64 cores per
    group. For >64 cores, GroupAffinity APIs should be used (future extension).

  • Hyper-threading (SMT):
      Avoid pinning critical threads to sibling logical cores of the same
      physical core if latency determinism is required.

===============================================================================
*/

#include <cstdint>
#include <thread>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
    #include <sched.h>
#endif

namespace lcr::system {

//------------------------------------------------------------------------------
// Thread priority abstraction
//------------------------------------------------------------------------------
//
// Minimal portable priority model:
//   - normal    → default OS scheduling
//   - high      → low-latency friendly
//   - realtime  → aggressive (may require privileges)
//
// Note:
// This is a best-effort mapping across platforms. Exact behavior depends on OS.
//
enum class thread_priority {
    normal,
    high,
    realtime
};

//------------------------------------------------------------------------------
// Pin current thread to a CPU core
//------------------------------------------------------------------------------
//
// Parameters:
//   core  → logical CPU index
//   prio  → desired scheduling priority (default: high)
//
// Returns:
//   true  → success
//   false → failure (affinity or priority could not be applied)
//
// Guarantees:
//   - noexcept
//   - no allocations
//   - safe for hot paths (though typically called during setup)
//
inline bool pin_thread(std::uint32_t core, thread_priority prio = thread_priority::high) noexcept {
#ifdef _WIN32

    // -------------------------------------------------------------------------
    // Windows implementation
    // -------------------------------------------------------------------------

    HANDLE thread = GetCurrentThread();

    // Set CPU affinity (bitmask: 1 << core)
    const DWORD_PTR mask = (1ull << core);

    if (SetThreadAffinityMask(thread, mask) == 0) {
        return false;
    }

    // Map abstract priority → Windows priority
    int win_prio = THREAD_PRIORITY_NORMAL;

    switch (prio) {
        case thread_priority::normal:
            win_prio = THREAD_PRIORITY_NORMAL;
            break;

        case thread_priority::high:
            win_prio = THREAD_PRIORITY_HIGHEST;
            break;

        case thread_priority::realtime:
            win_prio = THREAD_PRIORITY_TIME_CRITICAL;
            break;
    }

    if (!SetThreadPriority(thread, win_prio)) {
        return false;
    }

    return true;

#else

    // -------------------------------------------------------------------------
    // Linux implementation
    // -------------------------------------------------------------------------

    pthread_t thread = pthread_self();

    // Build CPU set
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    // Apply affinity
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return false;
    }

    // Best-effort priority mapping
    sched_param sch{};
    int policy = SCHED_OTHER;

    switch (prio) {
        case thread_priority::normal:
            policy = SCHED_OTHER;
            sch.sched_priority = 0;
            break;

        case thread_priority::high:
            policy = SCHED_FIFO;
            sch.sched_priority = 10; // safe mid-range
            break;

        case thread_priority::realtime:
            policy = SCHED_FIFO;
            sch.sched_priority = 80; // requires CAP_SYS_NICE
            break;
    }

    // Priority setting may fail without privileges → ignore result
    (void)pthread_setschedparam(thread, policy, &sch);

    return true;

#endif
}

//------------------------------------------------------------------------------
// Pin std::thread to a CPU core
//------------------------------------------------------------------------------
//
// Convenience wrapper for std::thread.
//
// Notes:
//   - Thread must be joinable
//   - Priority setting is NOT applied here (can be extended if needed)
//
inline bool pin_thread(std::thread& t, std::uint32_t core) noexcept {
#ifdef _WIN32

    if (!t.joinable()) return false;

    const DWORD_PTR mask = (1ull << core);

    HANDLE h = reinterpret_cast<HANDLE>(t.native_handle());

    return SetThreadAffinityMask(h, mask) != 0;

#else

    if (!t.joinable()) return false;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    return pthread_setaffinity_np(
        t.native_handle(),
        sizeof(cpu_set_t),
        &cpuset
    ) == 0;

#endif
}

} // namespace lcr::system
