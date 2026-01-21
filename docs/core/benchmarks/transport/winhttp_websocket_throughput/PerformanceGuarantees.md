# Transport Performance Guarantees

This document defines the **performance and stability guarantees** provided by
Wirekrak’s transport layer, based on empirical benchmarks and long-running
production-like workloads.

These guarantees describe what consumers of the transport layer can reasonably
expect in terms of throughput, efficiency, and reliability.

---

## Scope

These guarantees apply to:
- Transport-level WebSocket implementations
- RX/TX behavior on the wire
- Message delivery mechanics

They do **not** cover:
- Protocol parsing
- Order book processing
- Application-level logic
- End-to-end latency guarantees

---

## Stability Guarantees

- WebSocket connections are expected to remain stable under continuous,
  high-volume workloads for multi-hour durations.
- No unbounded memory growth or resource leaks under sustained load.
- Graceful handling of fragmented and non-fragmented messages.
- No implicit retries, reconnects, or hidden lifecycle transitions.

---

## Throughput Guarantees

- Transport throughput is bounded primarily by upstream feed characteristics,
  not by transport-layer inefficiencies.
- Sustained RX rates in excess of **100 KB/s** are supported under real Kraken
  book workloads without degradation.
- TX overhead is minimal and typically limited to control or subscription
  messages.

---

## RX Path Efficiency Guarantees

- The receive path is optimized for the common case:
  - **>99.5%** of messages are delivered via a zero-copy fast path.
- Fragmented messages are handled via an explicit assembly path.
- Fragmentation handling does not materially impact overall throughput.

---

## Assembly Cost Guarantees

- Message assembly cost is bounded and non-accumulative.
- Average assembly cost remains in the low microsecond range per fragmented
  message.
- Total time spent assembling messages over long runs is negligible relative
  to total runtime.

---

## Observability Guarantees

- Transport behavior is observable via snapshot-based telemetry.
- Metrics collection is lock-free and designed for minimal overhead.
- Telemetry levels allow fine-grained diagnostics without impacting baseline
  performance.

---

## Architectural Notes

- Guarantees are based on measured behavior, not theoretical limits.
- Results are backend- and configuration-dependent.
- New transport backends are expected to meet or exceed these guarantees.

---

## Disclaimer

These guarantees reflect observed behavior under specific workloads and
environments. Actual performance may vary based on network conditions,
hardware, operating system, and upstream data characteristics.

They are intended as **engineering guarantees**, not contractual SLAs.
