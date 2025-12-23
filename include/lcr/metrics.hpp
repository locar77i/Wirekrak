#pragma once

// ---------------------------------------------------------------------------
//  Metrics: Ultra-low-overhead telemetry for production HFT systems
// ---------------------------------------------------------------------------
//  • Uses monotonic_clock::now_ns() → no syscalls, ~10ns overhead.
//  • Purely atomic counters, relaxed memory order → safe for hot paths.
//  • Compile-time toggle (define ENABLE_FS_METRICS).
// ---------------------------------------------------------------------------

#include "lcr/metrics/constant_gauge.hpp"
#include "lcr/metrics/gauge.hpp"
#include "lcr/metrics/counter.hpp"
#include "lcr/metrics/latency_histogram.hpp"

#include "lcr/metrics/stats/sampler.hpp"
#include "lcr/metrics/stats/duration.hpp"
#include "lcr/metrics/stats/operation.hpp"
#include "lcr/metrics/stats/size.hpp"
#include "lcr/metrics/stats/life_cycle.hpp"

