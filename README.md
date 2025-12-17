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

---

## 🚀 Getting Started

### Prerequisites

- C++20 compatible compiler
- CMake ≥ 3.20
- Windows (WinHTTP transport)

### Build

```bash
git clone https://github.com/<your-org>/wirekrak.git
cd wirekrak
cmake -S . -B build
cmake --build build
```

---

## 📡 Example Usage

```cpp
#include <wirekrak/client.hpp>

using namespace wirekrak;

int main() {
    Client client({
        .on_trade = [](const trade::Event& trade) {
            std::cout << trade.symbol << " @ " << trade.price << std::endl;
        },
        .on_error = [](Error err) {
            std::cerr << err.message() << std::endl;
        }
    });

    client.subscribe_trades("BTC/USD");
    client.run();
}
```

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

## 🧪 Replay & Backtesting

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
