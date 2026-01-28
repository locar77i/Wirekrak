# Wirekrak ⚡  

## Deterministic Protocol Infrastructure for Kraken Market Data

**Wirekrak** treats the Kraken WebSocket API as a **deterministic market data pipeline**,
not a best-effort message stream.

Instead of exposing raw WebSocket frames or loosely typed callbacks,
Wirekrak provides **schema-first, strongly typed protocol-level market events
with explicit lifecycle and failure semantics**, designed for real-world trading
systems where correctness matters more than convenience or raw throughput.

> If your process crashes, disconnects, or restarts, Wirekrak makes it
> explicit *what you know, what you don’t, and how to recover safely*.

Wirekrak is implemented as a modern C++ WebSocket **infrastructure SDK**
for **low-latency, real-time trading applications** on the Kraken exchange.

It is closer in spirit to infrastructure like **Boost.Asio** than to a typical SDK:
it favors composition, explicit architectural boundaries, and strong invariants
over ad-hoc customization.

> 🚀 Built for the **Kraken Forge Hackathon** and evolving beyond it.
---

## Why Wirekrak Exists

Most WebSocket SDKs treat real-time data as an ephemeral stream:
messages arrive, callbacks fire, and correctness is assumed.

In real trading systems, this breaks down.

Crashes, reconnects, partial subscriptions, and protocol edge cases introduce
**implicit gaps** that application code must reason about manually —
often without clear guarantees.

**Wirekrak flips the model.**

It treats Kraken’s WebSocket API as **infrastructure**, with:
- explicit protocol state
- strongly typed schemas
- deterministic lifecycle management
- well-defined recovery boundaries

The result is a foundation you can reason about, test, and extend —
instead of a stream you hope behaves.

---

## 🧩 Problem Statement

Real-time trading systems require **deterministic behavior under failure**.

When integrating with the Kraken WebSocket API, developers must handle:
- reconnects and resubscriptions
- partial acknowledgements
- snapshot vs incremental state
- undocumented but stable protocol quirks

Most SDKs push these concerns into application code, making failure semantics
implicit and correctness accidental.

**Wirekrak makes failure semantics explicit** and enforces correctness by design —
not by convention.

By treating the Kraken WebSocket API as a **stateful system**, not as a message stream,
Wirekrak provides a schema-first, strongly typed interface with explicit lifecycle
management, deterministic recovery boundaries, and a clean separation between
transport, protocol, and client policy.

> Wirekrak defines clear recovery boundaries today, and is architected so that
> durable replay and persistence are natural extensions rather than retrofits.

---

## 🧱 Architecture Overview <a name="architecture"></a>

Wirekrak is organized as a set of **strict, intentionally incomplete layers** with
explicit responsibilities, clear dependency direction, and well-defined failure
boundaries.

The architecture prioritizes **determinism, protocol correctness, and semantic
clarity** over ad-hoc flexibility or implicit behavior. Each layer serves a
different audience and must not compensate for responsibilities owned by another.

```
wirekrak/
├──core/
├   ├── transport/                      # Protocol-agnostic WebSocket interfaces
├   ├── stream/                         # Client Policy — reconnection, liveness
├   ├── protocol/kraken/                # Kraken-specific protocol handling
├               ├── schema/             # Strongly typed Kraken message schemas
├               ├── parser/             # Modular message parsing layer
├               ├── channel/            # Channel subscription manager
├               ├── replay/             # Deterministic subscription replay engine
│
├── market/       (future)              # Semantic market-data layer
│   ├── streams/                        # Trades, order books, tickers
│   ├── policy/                         # Correctness & liveness policies
│   ├── state/                          # state machines (Snapshot–delta)
│
├── lite/                               # User-facing SDK layer
│   ├── client/                         # High-level client API
│   ├── domain/                         # Stable, simplified data models
│
├── lcr/                                # Low-latency common resources
│
├── examples/                           # Usage examples (Lite-focused)
├── benchmarks/                         # Performance benchmarks
└── tests/                              # Unit & integration tests
```

### Layer relationships

- **Core** is the single source of protocol truth and lifecycle semantics
- **Market** depends on Core and encodes *semantic correctness*
- **Lite** depends on Core and provides *conservative, exchange-agnostic access*
- **Market and Lite are peers** and must not depend on each other
- **Core must never depend on higher layers**

Using the wrong layer for a given problem is considered a design error.

➡️ **[Architecture Overview](docs/ARCHITECTURE.md)**

---

## 🧠 Design Guarantees & System Properties

Wirekrak is designed around explicit system guarantees rather than
best-effort behavior. Each layer enforces clear responsibilities,
making correctness observable and testable.

### 1️⃣ Schema-First, Strongly Typed Protocol

All Kraken WebSocket messages are modeled using explicit schemas.

- Strongly typed request, response, and event models
- No fragile string-based JSON handling
- Compile-time validation of channel payloads
- Clear separation between schema definition and transport

This eliminates entire classes of runtime errors and enforces protocol correctness.

---

### 2️⃣ Deterministic Lifecycle & Failure Semantics

Wirekrak explicitly models:

- subscription acknowledgements
- snapshot vs incremental state
- reconnect and resubscription behavior
- stalled or dead connection detection

Failure is **modeled**, not ignored.

---

### 3️⃣ Transport-Agnostic Infrastructure Core

WebSocket transport is a replaceable detail.

- Protocol-agnostic transport interfaces
- Current implementation: Windows WinHTTP WebSocket
- Easily portable to Boost.Asio, libwebsockets, etc.
- Fully mockable for deterministic testing

Transport failures are surfaced explicitly, not hidden.

---

### 4️⃣ Kraken-Specific Protocol Correctness

The Kraken WebSocket API is modeled as a first-class protocol, not a generic stream.

- Typed channel traits for Kraken subscriptions
- Explicit routing of incoming messages
- Validation of protocol invariants
- Clean mapping from raw messages → typed domain events

This avoids “one-size-fits-all” SDK design and preserves correctness.

---

### 5️⃣ Deterministic, Test-Driven Architecture

System behavior is verified through automated tests across all layers:

- protocol validation tests
- transport behavior tests
- liveness and reconnection tests

Correctness is demonstrated, not assumed.

---

### 6️⃣ Strict Separation of Concerns

Wirekrak enforces a clean architectural split:

- **Transport** — connectivity and failure signaling
- **Client Policy** — reconnection and liveness decisions
- **Protocol** — schemas, routing, and validation

This enables deterministic testing, clean failure handling,
and future multi-exchange support.

---

## 🚀 Getting Started

### Prerequisites

- C++20 compatible compiler
- CMake ≥ 3.25
- Windows (WinHTTP transport)

### Installation <a name="installation"></a>

Before building Wirekrak, please install the required dependencies using **vcpkg**.

➡️ **[Install dependencies](docs/build/INSTALL_DEPENDENCIES.md)**

This guide covers:
- vcpkg setup
- Required libraries (simdjson, spdlog, CLI11)
- CMake presets
- Debug / Release builds

### 🔧 Build

```bash
git clone https://github.com/<your-org>/wirekrak.git
cd wirekrak
cmake -S . -B build
cmake --build build
```

---

## 📡 Example Usage

If you just want Kraken V2 market data in fewer lines of C++, start here:

```cpp
#include "wirekrak.hpp"
using namespace wirekrak::lite;

int main() {
    // Client setup (Lite SDK)
    Client client{"wss://ws.kraken.com/v2"};

    // Optional error handling
    client.on_error([](const Error& err) {
        std::cerr << "[wirekrak-lite] error: " << err.message << std::endl;
    });

    // Connect
    if (!client.connect()) {
        return -1;
    }

    // Subscribe to BTC/USD trades (snapshot + updates)
    client.subscribe_trades({"BTC/USD"},
        [](const Trade& trade) {
            std::cout << " -> " << trade << std::endl;
        },
        true // snapshot
    );

    // Main polling loop
    while (!Ctrl_C) {
        client.poll();   // REQUIRED to process incoming messages
    }

    client.unsubscribe_trades({"BTC/USD"});
    return 0;
}

```

```Note:``` Lite exposes snapshot and update delivery, but does not validate snapshot–delta
consistency or provide semantic correctness guarantees.

Wirekrak is designed to support ergonomic application-level consumption through the Lite layer,
that provides a stable, callback-based API for consuming Kraken market data with explicit
snapshot and lifecycle handling:

➡️ **[Wirekrak Lite - Quick Usage Guide](docs/lite/QUICK_USAGE.md)**

```Note:``` This guide assumes no prior knowledge of Wirekrak internals and focuses on the most common usage patterns.

For ultra-low-latency or protocol-level control, applications may use Wirekrak Core directly,
which exposes strongly typed protocol requests constrained by C++20 concepts.

---

## 🧱 Build Presets <a name="cmake-presets"></a>

Wirekrak uses **CMake Presets** to make build intent explicit.

- **`ninja-core-only`** builds *Core only*: header-only, no Lite, no examples, no tests, no telemetry — recommended for **ULL / infrastructure use**.
- Example and integrations presets explicitly enable **Wirekrak Lite** and higher-level SDK features.
- Benchmark presets are isolated to avoid accidental coupling with SDK or telemetry logic.

➡️ **[Detailed preset guide](docs/build/CMAKE_PRESETS.md)**

Use presets to choose what you build, not just how you build.

---

## 🧪 Running Tests

Wirekrak is backed by automated tests that verify deterministic behavior
across protocol handling, liveness detection, and reconnection logic.

Tests are built and executed using **CMake Presets** and **CTest**.

### Quick Start

```bash
cmake --preset ninja-debug
cmake --build --preset debug
ctest --preset test-debug
```

**Notes**

- Tests cover protocol validation, heartbeat timeouts, and reconnection behavior
- No external test frameworks are required
- Test execution is fully deterministic and reproducible
---

## ⚙️ Usage Patterns

Wirekrak supports **two distinct usage paths**, depending on what you want
to build and how deep you want to go.

### Core SDK — Full Infrastructure Control <a name="core-examples"></a>

The Core SDK exposes Wirekrak’s **transport, connection, and protocol
foundations**.

If you are:
- integrating Wirekrak into infrastructure
- building your own protocols
- debugging lifecycle or liveness issues
- learning Wirekrak’s design philosophy

**Start here:**
➡️ [Wirekrak Core Examples](./docs/core/examples/README.md)

### Lite SDK — Fast & Safe Application-level Consumption <a name="lite-examples"></a>

The Lite SDK provides a **high-level, callback-based API** focused on
consuming market data with minimal setup.

If you are:
- consuming Kraken market data
- building trading or analytics applications
- uninterested in transport internals
- prioritizing speed and ergonomics

**Start here:**
➡️ [Wirekrak Lite Examples](./docs/lite/examples/README.md)  

---

## 🧠 End-to-End System Integration (Flashstrike) <a name="integrations"></a>

Wirekrak was designed not only as a WebSocket SDK, but as a reliable
**market-data ingestion layer** within larger trading systems.

To validate this design, Wirekrak includes an **opt-in end-to-end integration**
with *Flashstrike* — a high-performance matching engine developed independently
prior to the hackathon.

This integration is intentionally isolated from the core library and
serves as **architectural validation**, not a required dependency.

### ⚡ Flashstrike — Exchange-Grade Matching Engine <a name="flashstrike"></a>

**Flashstrike** is a high-performance matching engine designed for
ultra-low-latency trading systems. It demonstrates exchange-grade design
principles including:

- deterministic order matching
- explicit state transitions
- cache-efficient data structures
- predictable latency under load

➡️ **[Flashstrike documentation](./docs/integrations/Flashstrike/README.md)**

The Wirekrak ↔ Flashstrike integration demonstrates how real-time Kraken
market data can be normalized, validated, and injected into a deterministic
execution engine — mirroring real-world exchange ingestion pipelines.

### Kraken → Flashstrike Gateway

The integration gateway layer performs:

- normalization of Kraken market data
- translation of book updates into deterministic limit orders
- strict separation between ingestion and execution domains

This mirrors production exchange architectures, where market data ingestion
and matching engines evolve independently but communicate through well-defined
interfaces.

➡️ **[flashstrike Gateway Example](./docs/integrations/KrakenFlashstrikeGateway.md)**

Together, these components demonstrate how Wirekrak fits naturally
into production-style trading architectures beyond standalone data consumption.

> ⚠️ Scope note  
> Flashstrike is not part of the Wirekrak SDK and is not required to use it.
> The integration exists solely to validate Wirekrak’s architectural
> assumptions and demonstrate end-to-end system design.

---

## 📊 Benchmarks <a name="benchmarks"></a>

Wirekrak includes a growing set of **layered performance benchmarks** designed to
measure isolated system behavior with clear scope and reproducible results.

Benchmarks are organized by subsystem (transport, protocol, stream, end-to-end) and
each lives in its own directory alongside source code, configuration, and documented
observations.

➡️ **[Benchmarks Overview](./docs/core/benchmarks/README.md)**

---

## ⚠️ Notes on Kraken WebSocket Behavior

While testing against the live Kraken WebSocket API, a few minor differences between the documentation and actual behavior were observed.
Wirekrak handles these cases explicitly to ensure reliable operation.

### pong responses

Kraken sometimes sends lightweight pong heartbeat messages without success or result fields.
These messages are valid and indicate a healthy connection.

- Wirekrak accepts both documented and heartbeat-style pong responses.

### Subscribe / Unsubscribe errors

When a subscription request fails (e.g. duplicate subscription), Kraken returns an error-only response with success = false and no result object.

- Wirekrak requires result only on successful responses and correctly parses error replies.

### Book snapshot vs updates

The Kraken book channel delivers a full snapshot followed by incremental updates within the same subscription. Snapshot messages may include undocumented but stable fields (e.g. timestamp), which are treated as metadata and ignored for book logic.

- Wirekrak tolerates extra fields for forward compatibility.

---

## ✨ Acknowledgements

Built with caffeine, curiosity, and a little help from ChatGPT.

ChatGPT was a great brainstorming partner throughout the hackathon, helping explore ideas, refine approaches, and keep the project moving forward.

---

## 📜 License

MIT License

---

## 🤝 Contributing

Contributions are welcome!  
Please open an issue or submit a pull request.

---

**Built with ❤️ for real-time trading**
