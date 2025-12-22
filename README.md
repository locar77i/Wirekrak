# WireKrak ⚡  
**A High-Performance WebSocket SDK for Real-Time Kraken Trading**

WireKrak is a modern C++ WebSocket SDK designed for **low-latency, real-time trading applications** on the Kraken exchange.  
It is built with **production-grade architecture**, focusing on reliability, determinism, and extensibility.

> 🚀 Built for the **Kraken Forge Hackathon**

---

## ✨ Key Features

- **Schema-first design**  
  Strongly typed request/response and event models (no fragile string-based JSON handling).

- **Transport abstraction**  
  Clean separation between WebSocket transport and protocol logic, enabling portability and testability.

- **Write-Ahead Logging (WAL)**  
  Persist incoming events for **auditability, recovery, and deterministic replay**.

- **Deterministic Replay Engine**  
  Replay historical WebSocket sessions for backtesting, debugging, and simulation.

- **Low-latency focus**  
  Designed with trading workloads in mind: minimal allocations, predictable execution paths.

- **Extensible architecture**  
  Easily adaptable to additional exchanges or protocols.

---

## 🧱 Architecture Overview

WireKrak is structured in three independent layers:

• Transport — WebSocket connectivity and failure signaling  
• Client Policy — reconnection, liveness, and subscription management  
• Protocol — Kraken-specific message schemas and routing  

This separation allows deterministic testing, clean failure handling,
and future support for additional exchanges or transports.

```
wirekrak/
├── core/transport/     # Protocol-agnostic WebSocket interfaces
├── winhttp/            # Windows WebSocket transport implementation
├── protocol/kraken/    # Kraken-specific protocol handling
├── schema/             # Strongly typed Kraken message schemas
├── wal/                # Write-ahead logging & persistence
├── replay/             # Deterministic replay engine
└── examples/           # Usage examples
```

**Design principles:**
- Clear separation of concerns
- No transport-specific leakage into domain logic
- Deterministic, replayable event streams

### Transport Architecture <a name="transport"></a>

- Transport is fully decoupled from protocol logic via C++20 concepts
- WebSocket and WinHTTP APIs are modeled as compile-time contracts
- Enables mocking, testing, and reuse with zero runtime overhead

➡️ **[Transport overview](docs/architecture/transport/Overview.md)**

---

### Threading Model <a name="threading-model"></a>

Wirekrak currently uses a deliberate **2-thread architecture** optimized for
low latency and correctness. A 3-thread model (dedicated parser thread) is
planned but intentionally postponed until full benchmarking is completed.

➡️ **[Read the full threading model rationale](docs/notes/ThreadingModel_2T_VS_3T.md)**

---

### Low-latency Common Resources (`lcr`)

WireKrak includes a small internal utility layer named **LCR** (Low-latency Common Resources).

`lcr` contains reusable, header-only building blocks designed for
low-latency systems and used across ULL systems, including:

- lock-free data structures
- lightweight `optional` and helpers
- bit-packing utilities
- logging abstractions

These utilities are **domain-agnostic** (not Kraken-specific) and are intentionally kept separate
from the WireKrak protocol code to allow reuse across other ULL (Ultra-Low Latency) projects.

WireKrak itself depends on `lcr`, but `lcr` does not depend on WireKrak.

---

## 🚀 Getting Started

### Prerequisites

- C++20 compatible compiler
- CMake ≥ 3.25
- Windows (WinHTTP transport)

### Installation <a name="installation"></a>

Before building Wirekrak, please install the required dependencies using **vcpkg**.

➡️ **[Install dependencies](INSTALL_DEPENDENCIES.md)**

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

```cpp
#include "wirekrak/client.hpp"

using namespace wirekrak;

int main() {
    
    // Client setup
    WinClient client;

    // Register handlers
    client.on_pong(...);
    client.on_status(...);
    client.on_rejection(...);

    // Connect
    if (!client.connect("wss://ws.kraken.com/v2")) {
        return -1;
    }

    // Subscribe to BTC/USD trades with snapshot enabled
    client.subscribe(trade::Subscribe{.symbols = {"BTC/USD"}, .snapshot = true},
                     [](const trade::Trade& msg) { std::cout << " -> " << msg << std::endl; }
    );

    // Main polling loop
    while (!Ctrl_C) {
        client.poll();   // REQUIRED to process incoming messages
    }

    client.unsubscribe(trade::Unsubscribe{.symbols = {"BTC/USD"}});
    
    return 0;
}

```

```Note:``` Subscribe, Unsubscribe, and Control requests are modeled as distinct types and constrained using C++20 concepts, ensuring request misuse fails at compile time with zero runtime overhead.

---

## ✨ Liveness Detection

WireKrak uses dual liveness detection: protocol heartbeats and real message flow.
Reconnection only occurs when both signals stop, preventing false reconnects during quiet markets.”

### Heartbeats monitoring

WireKrak actively monitors protocol heartbeats.
If heartbeats stop beyond a configurable timeout, the client
assumes the connection is unhealthy and signal it to reconnect.

### Messages monitoring

WireKrak actively monitors protocol messages. If last received message timestamp is greather than
a configurable timeout, the client assumes the connection is unhealthy and and signal it to reconnect.

---

## 🔬 Protocol-Strict, Low-Latency Parser

Wirekrak includes a production-grade WebSocket parser designed for real-time market data.
It uses schema-strict validation, constexpr-based enum decoding, and zero-allocation parsing on top of simdjson.

### Overview

The architecture cleanly separates routing, parsing, and domain adaptation, enforcing real exchange semantics (e.g. snapshot vs update invariants) and rejecting malformed messages deterministically.

#### Parser Router

Incoming WebSocket messages are first routed by method/channel to the appropriate message parser, ensuring each payload is handled by the correct protocol-specific logic with minimal branching.

#### Layered Parsers (Helpers → Adapters → Parsers)

Low-level helpers validate JSON structure and extract primitives, adapters perform domain-aware conversions (enums, symbols, timestamps), and message parsers handle control flow and logging. This separation keeps parsing fast, safe, and maintainable.

#### Explicit Error Semantics

Parsing distinguishes between invalid schema and invalid values using a lightweight enum, allowing robust handling of real-world Kraken API inconsistencies while remaining allocation-free and exception-free.

```note``` Every parser is fully unit-tested against invalid, edge, and protocol-violating inputs, making refactors safe and correctness provable.

```Built as infrastructure, not a demo.```

---

## 🔁 Replay & Backtesting

WireKrak supports **deterministic replay** of recorded WebSocket sessions:

- Record live market data via WAL
- Replay offline with identical event ordering
- Ideal for:
  - Strategy backtesting
  - Incident analysis
  - Debugging race conditions

```bash
wirekrak-replay --input wal.bin --speed 1.0
```

---

## 🔐 Authentication

Private feeds use Kraken WebSocket authentication:
- API key & secret never leave the client
- Nonce and signature generation handled internally

```cpp
client.authenticate(api_key, api_secret);
```

---

## 🧪 Running Tests

WireKrak uses **CMake Presets** and **CTest** for building and running tests.
All test configuration is handled automatically via presets.

### Prerequisites
- CMake ≥ 3.23
- Ninja
- A C++20-compatible compiler

### Configure

**Debug**
```bash
cmake --preset ninja-debug
```

```bash
cmake --preset ninja-release
```

This generates build directories under:

- build/debug
- build/release

### Build

```
cmake --build --preset debug
```

```
cmake --build --preset release
```

###  Run Tests

```
ctest --preset test-debug
```


```
ctest --preset test-release
```

By default, test output is shown only on failure.

### Verbose Test Output

```
ctest --preset test-debug --verbose
```

or:

```
ctest --preset test-debug -VV
```

### Run a Specific Test

```
ctest --preset test-debug -R LivenessTest
```

### Notes

- Tests are enabled via the WK_UNIT_TEST=ON preset option.
- Unit tests cover client liveness detection, heartbeat and message timeouts, and automatic reconnection behavior.
- No external test frameworks are required.

---

## Examples <a name="examples"></a>

### Wirekrak Trades

This example demonstrates how to subscribe to Kraken trade events using WireKrak, supporting one or multiple symbols,
optional snapshot mode, and clean shutdown via Ctrl+C:

➡️ **[Trade subscription (single & multi-symbol)](./docs/examples/Trades.md)**

### Wirekrak Book Updates

This example demonstrates how to subscribe to Kraken order book updates using WireKrak, supporting one or multiple symbols, clean shutdown via Ctrl+C, and rejection handling.

➡️ **[Book update subscription (single & multi-symbol)](./docs/examples/BookUpdates.md)**

---

## ⚠️ Notes on Platform Behaviors <a name="platform-notes"></a>

➡️ **[WinHTTP WebSocket close delay on Windows](./docs/notes/WinhttpCloseDelay.md)**

On Windows, WebSocket closure via WinHTTP/SChannel is asynchronous and may take
~10 seconds to fully complete. This is expected OS behavior and does not affect
Wirekrak’s reconnection logic.

---

## ⚠️ Notes on Kraken WebSocket Behavior

While testing against the live Kraken WebSocket API, a few minor differences between the documentation and actual behavior were observed.
WireKrak handles these cases explicitly to ensure reliable operation.

### pong responses

Kraken sometimes sends lightweight pong heartbeat messages without success or result fields.
These messages are valid and indicate a healthy connection.

- WireKrak accepts both documented and heartbeat-style pong responses.

### Subscribe / Unsubscribe errors

When a subscription request fails (e.g. duplicate subscription), Kraken returns an error-only response with success = false and no result object.

- WireKrak requires result only on successful responses and correctly parses error replies.

---

## 🧠 Why This Matters

Traditional WebSocket examples focus on connectivity.

WireKrak focuses on:
- **Correctness under failure**
- **Recoverability**
- **Deterministic behavior**
- **Production trading constraints**

This makes it suitable for:
- Algo trading systems
- Market data pipelines
- Trading dashboards
- Simulation & research environments

---

## 🏁 Hackathon Highlights

- Schema-driven protocol layer
- WAL-backed market data ingestion
- Deterministic replay for backtesting
- Production-inspired SDK design

---

## 📜 License

MIT License

---

## 🤝 Contributing

Contributions are welcome!  
Please open an issue or submit a pull request.

---

**Built with ❤️ for real-time trading**
