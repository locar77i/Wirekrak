# `monotonic_clock` Documentation

This document describes the Flashstrike system-level timing utilities:
- `tsc_calibrator`
- `rdtsc()`
- `tsc_to_ns()`
- `calibrate_tsc()`
- `monotonic_clock` (singleton nanosecond-precision timestamp source)

---

# Overview

Flashstrike uses the CPU's **Time Stamp Counter (TSC)** to provide:
- high‑resolution timestamps,
- extremely low latency (nanosecond-scale),
- monotonicity guarantees,
- optional runtime recalibration.

The `monotonic_clock` class converts raw TSC cycles into **strictly monotonic UTC‑nanosecond timestamps**, suitable for:
- order sequencing,
- event timing,
- latency measurement,
- matching engine logging.

---

# `tsc_calibrator`

```cpp
struct tsc_calibrator {
    uint64_t base_tsc;          // Reference TSC at calibration
    uint64_t base_wallclock_ns; // Wall-clock time in ns at calibration
    double tsc_freq;            // Measured cycles per second
};
```

A calibration snapshot containing:
- the TSC value at calibration,
- the corresponding wall‑clock time in nanoseconds,
- the measured TSC frequency.

This allows conversion from:
```
TSC cycles → elapsed ns → wall‑clock timestamps
```

---

# Reading the TSC (`rdtsc()`)

```cpp
inline uint64_t rdtsc() noexcept;
```

Direct assembly instruction (`RDTSC`), returning a **64‑bit cycle count**.

Properties:
- extremely fast (sub‑nanosecond),
- CPU-local, not synchronized across sockets,
- non‑serializing (can reorder slightly).

---

# Converting TSC to Nanoseconds

```cpp
inline uint64_t tsc_to_ns(uint64_t tsc, const tsc_calibrator& calib) noexcept;
```

Conversion steps:
1. Compute cycle delta from base calibration.
2. Convert cycles → seconds using `tsc_freq`.
3. Convert seconds → nanoseconds.
4. Add `base_wallclock_ns`.

Result: **UTC nanoseconds**.

---

# TSC Calibration

```cpp
inline tsc_calibrator calibrate_tsc(unsigned int sleep_ms = 50);
```

Calibration procedure:
1. Record steady‑clock timestamp (`t1`)
2. Read `tsc1`
3. Sleep for a fixed duration
4. Record steady‑clock timestamp (`t2`)
5. Read `tsc2`
6. Estimate TSC frequency:
   ```
   freq = (tsc2 - tsc1) / elapsed_seconds
   ```
7. Sample the system wall‑clock time

The result is a calibrated `(base_tsc, base_wallclock_ns, tsc_freq)` triple.

---

# `monotonic_clock`

A **Meyers Singleton** providing strictly monotonic nanosecond timestamps.

## Access

```cpp
static monotonic_clock& instance();
```

## `now_ns()`

```cpp
uint64_t now_ns() noexcept;
```

Returns a **strictly increasing** timestamp in nanoseconds.

Algorithm:
1. Load current calibration
2. Convert `rdtsc()` → UTC ns
3. Compare against last returned value
4. If new timestamp ≤ previous, bump to `prev + 1`
5. Store and return

Guarantees:
- no backwards jumps
- no duplicates
- safe under concurrency (atomic updates)

---

# Background Recalibration

### Start automatic recalibration

```cpp
void start_recalibration(std::chrono::seconds interval);
```

A background thread:
- sleeps for `interval`,
- recalibrates TSC,
- updates the shared calibration pointer.

This corrects:
- drift,
- thermal effects,
- CPU frequency scaling.

### Stop recalibration

```cpp
void stop_recalibration();
```

Gracefully shuts down the calibration thread.

---

# Internal Fields

- `calib_ptr_`: atomic shared pointer to the calibration record  
- `last_ns_`: enforces strict monotonicity  
- `running_`: controls recalibration thread  
- `recal_thread_`: background worker  

---

# Design Goals

- **Ultra‑low latency**: `now_ns()` is roughly just RDTSC + multiply/add
- **Strict monotonicity**: guaranteed increasing timestamps
- **Lock‑free**: no mutexes in the hot path
- **Multicore safe**
- **Drift-compensated**: recalibrates at runtime

---

# Example Usage

```cpp
using flashstrike::system::monotonic_clock;

uint64_t t1 = monotonic_clock::instance().now_ns();
// perform work
uint64_t t2 = monotonic_clock::instance().now_ns();

std::cout << "Elapsed ns: " << (t2 - t1) << std::endl;
```

---

# Summary

`monotonic_clock` is a high‑performance timing subsystem providing:
- nanosecond timestamps from TSC,
- monotonic ordering,
- runtime recalibration,
- atomic lock‑free reads.

It is the foundation for timing and sequencing inside Flashstrike.

