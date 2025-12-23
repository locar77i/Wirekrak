#pragma once

#include <thread>  // std::this_thread::yield fallback


// -----------------------------------------------------------------------------
// Portable spin-wait hint
// _mm_pause() itself is not portable — it’s an x86/x86-64 intrinsic.
// On ARM, we use the "yield" instruction via inline assembly.
// On other platforms, we fallback to std::this_thread::yield().
// -----------------------------------------------------------------------------
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h> // _mm_pause
#elif defined(__aarch64__) || defined(__arm__)
    #include <arm_acle.h>  // __yield
#endif


// Spinlock constants
constexpr uint32_t SPINS_GUESS = 256;      // initial guess for adaptive busy-wait
constexpr uint32_t MIN_SPINS_LIMIT = 16;   // absolute minimum spins for adaptive busy-wait
constexpr uint32_t MAX_SPINS_LIMIT = 1024; // absolute maximum spins for adaptive busy-wait


namespace lcr {
namespace system {


inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__) || defined(__arm__)
    asm volatile("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

} // namespace system
} // namespace lcr
