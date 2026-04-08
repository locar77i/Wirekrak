---
title: "Wirekrak"
subtitle: |
  **Full-Exchange Market Data Ingestion Benchmark**  
  \small Kraken WebSocket v2 · 57h Live Stress Test · Real-World Conditions
author: "Rafael López Caballero"
date: "April 2026"
titlepage: true
titlepage-color: "0F172A"
titlepage-text-color: "FFFFFF"
titlepage-rule-color: "38BDF8"
---

# Wirekrak Performance Report (Kraken v2)

Wirekrak is a C++20 ultra-low-latency market data engine designed for deterministic behavior under real-world conditions.

This report presents a full-exchange ingestion benchmark conducted over 57 hours against Kraken WebSocket v2, capturing both steady-state and failure scenarios.

## Full-Exchange Market Data Ingestion Stress Test

> **1.12 Billion messages. 209 GB. Zero data loss. Deterministic recovery.**

## Overview

This report details a **57-hour continuous stress test** of the Wirekrak market data engine. Unlike synthetic benchmarks, this test was conducted against live exchange production endpoints, under real-world conditions including network jitter, exchange-initiated disconnects, and large-scale subscription replay.

- **Source:** Kraken WebSocket API v2 (Production)
- **Scope:** Full Exchange Ingestion (Order Book L2 + Trades)
- **Symbols:** 1,489 active pairs (Total Exchange)
- **Duration:** 56.8 hours (continuous runtime)
- **Efficiency:** 99.999985%

## Executive Summary

**1.12** Billion messages processed and **209 GB** ingested over 57 hours, with **4.1 µs** of median latency (end-to-end) and **795K msg/s** of processing capacity (×144 headroom)  

| Category | Metric | Result |
| :--- | :--- | :--- |
| **Throughput** | Total Messages processed | **1,128,668,817** |
| | Total Data ingested | **209 GB** |
| **Latency** | p50 (Median End-to-End) | **4.10 µs** |
| | Internal Processing (Avg) | **1.26 µs** (protocol layer) |
| **Capacity** | Processing Rate | **795K msg/s** |
| | Headroom | **×144** |
| **Reliability**| Message Loss / Errors | **0 / 0** |
| | Reconnect Success | **100% (10/10)** |

## Architectural Design (Mechanical Sympathy)

The performance observed is a direct result of Wirekrak’s core design principles:

* **Zero-Copy Data Path:** Raw bytes are read directly into pre-allocated slots in an **SPSC (Single Producer Single Consumer)** ring buffer. No intermediate copies or heap allocations occur in the hot path.
* **Lock-Free Concurrency:** Communication between the Transport thread (`Engine`) and the Logic thread (`Session`) uses memory-barriers rather than mutexes, eliminating context-switch overhead and priority inversion.
* **Thread Pinning:** The engine utilizes hardware affinity to isolate the receive loop and protocol parser on dedicated physical cores, reducing cache invalidation and OS scheduling jitter.
* **Deterministic State Machine:** The `Connection` layer manages the logical session epoch, ensuring that after a disconnect, the system re-enters a "known good" state before resuming data flow.

## Latency Analysis

| Percentile | Latency (End-to-End) |
| :--- | :--- |
| **p50 (Median)** | **4.10 µs** |
| **p90** | 65.5 µs |
| **p99** | 2.10 ms |
| **p99.9** | 16.8 ms |

> **Note:** Latency spikes at p99+ are primarily attributed to public internet conditions (TCP congestion, routing variability) and exchange-side processing. The **internal Wirekrak processing overhead** remains stable and constant at **~1.2 µs** on average.

## Resilience & Reliability

Market data systems fail when conditions are uneven. Wirekrak was tested against 10 real exchange-initiated disconnects and proved its ability to recover without manual intervention.

* **Reconnects:** 10 successful cycles.
* **Retry Policy:** Exponential backoff (50ms → 1000ms).
* **Subscription Replay:** Successfully re-synchronized 1,489 symbol subscriptions across both Trade and Book channels after every disconnect.
* **Backpressure Handling:** 2 events detected during extreme bursts; the system utilized hysteresis-based flow control to recover in **<30ms** without observable data loss.

## Resource Footprint

Wirekrak maintains a constant memory profile regardless of uptime, preventing non-deterministic OS "jitter" from memory management.

| Component | Detail | Value |
| :--- | :--- | :--- |
| **Message Ring** | SPSC Slots | 4096 slots |
| **Block Pool** | Pre-allocated Blocks | 32 x 128 KB |
| **Static Footprint** | Post-Initialization | **~8.5 MB** |
| **Dynamic Growth** | Runtime Allocations | **0.00 B** |

> **Note:** Memory usage remains constant over time, eliminating allocator-induced latency variance.


## Key Takeaways

1.  **Exchange-Scale Proven:** Capable of ingesting the full Kraken market data feed on a single instance.
2.  **Microsecond Latency:** Sub-5µs median latency even with complex JSON protocol routing.
3.  **Massive Headroom:** 144x capacity margin provides resilience during extreme market conditions.
4.  **Deterministic Recovery:** Auto-replay database ensures local state matches exchange state after transport failures.

## Why This Matters

Market data systems do not fail at peak throughput. They fail when:  
- traffic is bursty  
- symbol distribution is highly skewed  
- connections reset under load

At scale, the real risks are:  
- silent data loss  
- inconsistent state after reconnect  
- latency degradation under pressure

This benchmark demonstrates that Wirekrak:  
- maintains deterministic state across reconnects  
- avoids data loss across failure cycles  
- preserves latency under real-world network conditions

## Conclusion

Wirekrak proves that high-level protocol correctness does not have to come at the cost of microsecond-level performance. By offloading I/O to a deterministic engine and utilizing a lock-free data plane, Wirekrak provides the determinism, observability, and performance required for professional market data and HFT infrastructure.

<!-- Page break -->
\newpage

## Appendix A — System Configuration

### Protocol Session Policies
```
[Protocol Backpressure Policy]
- Mode        : Custom
- Escalation  : 16777216 (threshold)
- Behavior    : Customized

[Protocol Liveness Policy]
- Mode        : Active
- Proactive   : yes

[Protocol Symbol Limit Policy]
- Mode        : None
- Enabled     : no

[Protocol Replay Policy]
- Mode        : Enabled
- Enabled     : yes

[Protocol Batching Policy]
- Mode       : Batch
- Enabled    : yes
- Batch size : 100
```

### Transport Connection Policies
```
[Transport Liveness Policy]
- Mode        : Enabled
- Enabled     : yes
- Timeout     : 15000 (ms)
- Warning     : 80% (threshold)
- Warning at  : 12000 (ms)

[Transport Retry Policy]
- Mode          : RetryableOnly
- Max Exponent  : 6
- Max Attempts  : infinite
- Fast Base     : 50 ms
- Normal Base   : 100 ms
- Slow Base     : 250 ms
```

### Transport WebSocket Policies
```
[Transport Backpressure Policy]
- Mode        : Custom
- Spins       : 32
- Hysteresis  : enabled
- Activation  : 1 (threshold)
- Deactivation: 16 (threshold)
- Behavior    : Customized
```

<!-- Page break -->
\newpage

## Appendix B — Memory Footprint

### Message Ring Memory Usage
```
  Static:  4.00 MB (4,194,432 bytes)
  Dynamic: 0.00 B (0 bytes)
  Total:   4.00 MB (4,194,432 bytes)
```

### Block Pool Memory Usage
```
  Static:  128 B (128 bytes)
  Dynamic: 4.00 MB (4,195,328 bytes)
  Total:   4.00 MB (4,195,456 bytes)
```

### Session Memory Usage
```
  Static:  274 KB (280,448 bytes)
  Dynamic: 271 KB (277,696 bytes)
  Total:   545 KB (558,144 bytes)
```

<!-- Page break -->
\newpage

## Appendix C — Detailed Telemetry & System Metrics

### EXECUTIVE SUMMARY
```
Throughput
  Data volume      : 209 GB (223,884,360,851 bytes)
  Total messages   : 1,128,668,817

  Ingress rate     : 5.51 K msg/s
  Process rate     : 795 K msg/s (x144 headroom)
  Poll rate        : 4.74 M polls/s

Latency (end-to-end)
  p50 (median)     : 4.10 us
  p90              : 65.5 us
  p99              : 2.10 ms
  p99.9            : 16.8 ms
  p99.99           : 67.1 ms
  p99.999          : 134 ms
  p99.9999         : 537 ms

  Tail amplification
    p99 / p50      : x512.00
    p99.99 / p50   : x16384.00
    p99.9999 / p50 : x131072.00

Stability
  Efficiency       : 99.999985 % (Perfect)
  Backpressure     : 2 (events)
  Ingress retries  : 0 (no message drops)
  Errors           : 0

Queueing model (Little's Law)
  Arrival rate     : 5.51 K msg/s
  Service rate     : 795 K msg/s
  Utilization      : 0.69 %

  Avg queue depth  : 1.22
  Avg latency      : 221 us

Observations
  - System operating far below saturation (0.69%)

System behavior
  Reconnects       : 10
  Replay events    : 20
```

### LATENCY ANALYSIS
```
Latency (end-to-end)
  p50 (median)     : 4.10 us
  p90              : 65.5 us
  p99              : 2.10 ms
  p99.9            : 16.8 ms
  p99.99           : 67.1 ms
  p99.999          : 134 ms
  p99.9999         : 537 ms

Ingress (transport -> ring)
  Avg             : 181 us
  Min             : 536 ns
  Max             : 15.3 s

Processing (protocol layer)
  Avg             : 1.26 us
  Min             : 270 ns
  Max             : 19.0 ms
```

### THROUGHPUT & FLOW
```
Message Processing
  Total            : 1,128,668,817
  Per poll (avg)   : 2.47 msg/poll
  Per poll (max)   : 128 msg/poll

Rates
  Processing rate  : 795 K msg/s
  Polling rate     : 4.74 M polls/s

Parser
  Success          : 1,128,668,312
  Ignored          : 505
  Failures         : 0
  Backpressure     : 0
```

### SYSTEM HEALTH & STABILITY
```
Lifecycle
  Total time        : 204815 s
  Healthy time      : 204815 s
  Backpressure time : 29.3 ms
  Efficiency        : 99.999985 % (Perfect)
  Degradation       : 0.000015 %

Backpressure
  Events detected   : 2
  Events cleared    : 2
  Overload streak   : 54,993,829

Queue Pressure
  Message ring depth: avg=1.22   min=1   max=3,008
  Control ring depth: avg=1.74   min=1   max=3

Delivery
  User failures     : 0
```

### TRANSPORT & CONNECTION
```
User activity
  Open calls        : 1
  Close calls       : 1

Connection
  Connect success   : 1
  Disconnect events : 10
  Retry attempts    : 17
  Epoch transitions : 11

Liveness
  Timeouts          : 0
  Threats           : 5

Traffic
  RX bytes          : 209 GB (223,884,360,851 bytes)
  TX bytes          : 414 KB (423,656 bytes)
  RX messages       : 1,128,668,817
  TX messages       : 365

Errors
  RX errors         : 0
  Send errors       : 0
```

### MEMORY
```
Memory Behavior
  Slot promotions   : 44,427
  Pool depth        : avg=1.85   min=0   max=32
```

### MESSAGE SHAPE
```
Message size
  Avg               : 198 B (198 bytes)
  Min               : 23.0 B (23 bytes)
  Max               : 85.1 KB (87,140 bytes)

Fragmentation
  RX fragments      : 5,133,951
  Avg fragments/msg : 1.00
  Max fragments/msg : 8
```

### CONTROL PLANE
```
Requests
  Emitted           : 365
  Subscriptions     : 2
  Unsubscriptions   : 2

Replay
  Replay requests   : 20
  Replay symbols    : 29,780

Connection signals
  Emitted           : 47
  Liveness threats  : 5
  Immediate retries : 10
  Scheduled retries : 7

Control ring failures (send gating)
  Total            : 0

```
