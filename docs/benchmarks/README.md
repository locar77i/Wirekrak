# Benchmarks

This directory contains **performance benchmarks** for Wirekrak.
Benchmarks are organized by **layer** and focus on measuring **isolated system behavior**
with clear scope and documented results.

Each benchmark is designed to be:
- reproducible
- architecture-aware
- independent of unrelated layers
- accompanied by source code and written observations

---

## Benchmark Structure

Benchmarks are grouped by subsystem:

```
benchmarks/
├── README.md
├── transport/
│   ├── README.md
│   └── <transport benchmarks>
├── protocol/
│   └── <protocol benchmarks>        (future)
├── stream/
│   └── <stream benchmarks>          (future)
└── end_to_end/
    └── <e2e benchmarks>             (future)
```

Each benchmark lives in its **own directory**, containing:
- the full benchmark source code
- configuration notes
- runtime output
- analysis and conclusions

This structure ensures long-term clarity and avoids mixing
code, results, and interpretation.

---

## Benchmark Index

### Transport Benchmarks <a name="transport-benchmarks"></a>
Benchmarks that measure **mechanical transport behavior**, independent of protocol
parsing or application logic.

- **WinHTTP WebSocket Throughput**  
  Measures RX/TX throughput, fragmentation behavior, fast-path efficiency,
  and message assembly cost for the WinHTTP WebSocket backend.

  ➡️ **[See Benchmark Result](transport/winhttp_websocket_throughput/README.md)**  

---

### Protocol Benchmarks *(planned)*
Benchmarks that measure **protocol-level costs**, such as parsing, validation,
and message dispatch.

Examples:
- JSON parsing throughput
- snapshot vs incremental update processing
- order book merge cost

---

### Stream Benchmarks *(planned)*
Benchmarks focused on **stream-level behavior**, including fan-out, backpressure,
and callback execution cost.

---

### End-to-End Benchmarks *(planned)*
Benchmarks measuring **full pipeline performance**, combining transport, protocol,
and stream layers to observe end-to-end latency and throughput.

---

## Benchmark Principles

All benchmarks in this repository follow these principles:

- One concern per benchmark
- No hidden work
- No implicit dependencies
- Telemetry-driven measurement
- Results documented alongside code

Benchmarks are not micro-optimizations; they are **tools for architectural validation**.

---

## Adding a New Benchmark

To add a new benchmark:

1. Choose the appropriate layer (`transport`, `protocol`, `stream`, `end_to_end`)
2. Create a dedicated directory
3. Include:
   - benchmark source code
   - a `README.md` describing setup and results
4. Add an entry to the relevant index

---

## Notes

Benchmark results are environment-dependent and reflect the
backend, configuration, and workload used at the time of execution.

They are intended as **comparative and diagnostic tools**, not absolute performance claims.

---

⬅️ [Back to README](../../README.md#benchmarks)
