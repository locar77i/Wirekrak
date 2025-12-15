# ⚡ Wirekrak C++ SDK
### *Ultra-fast, reliable, real-time WebSocket SDK for crypto trading systems*

Wirekrak is a high-performance C++20 SDK designed to consume real-time market data from cryptocurrency exchanges with **minimum latency**, **maximum reliability**, and **clean extensibility**.  
It is engineered using:

- **Boost.Beast** (WebSocket engine)  
- **Boost.Asio** (event loop, async I/O)  
- **simdjson** (ultra-fast JSON parsing)  
- **spdlog** (high-performance logging)  
- **Windows SChannel TLS** (native, zero-dependency wss:// support)

Wirekrak is built to integrate seamlessly into high-frequency trading systems and matching engines — including **FlashStrike**, your custom low-latency matching engine.

---

## 🚀 Features

### ⚡ Zero-lag WebSocket Streaming
Handles:
- Real-time trades  
- Orderbook updates  
- Tickers  
- Heartbeats  
- Channel subscriptions  

### 🛡 Automatic Reconnection
- Reconnects on failure  
- Restores subscriptions  
- Keeps state consistent  

### 🧠 Efficient JSON Pipeline (simdjson)
Gigabytes-per-second parsing throughput.

### 🔧 Windows-native TLS (SChannel)
No OpenSSL needed; zero external crypto dependencies.

---

## 📁 Project Structure

```
wirekrak/
├── include/wirekrak/
├── src/
├── examples/
├── tests/
├── docs/
└── CMakeLists.txt
```

---

## 🛠 Building Wirekrak

### Prerequisites
- CMake ≥ 3.25  
- MinGW-w64 (GCC 15.x)  
- vcpkg with installed packages:
  - boost-asio:x64-mingw-dynamic  
  - boost-beast:x64-mingw-dynamic  
  - simdjson:x64-mingw-dynamic  
  - spdlog:x64-mingw-dynamic  

### Configure & Build

```
cmake -B build -G "MinGW Makefiles" ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg-2024.11.16/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic

cmake --build build
```

---

## 🧪 Example: Connect & Receive Messages

```
#include <wirekrak/client.hpp>
#include <iostream>

int main() {
    Client client;

    // Subscribe to BTC/USD trades
    client.subscribe(schema::trade::Subscribe{.symbols = {"BTC/USD"}},
                     [](const schema::trade::Response& msg) {
                        std::cout << " -> [BTC/USD] TRADE: id=" << msg.trade_id << " price=" << msg.price << " qty=" << msg.qty << " side=" << to_string(msg.side) << std::endl;
                     }
    );

    client.connect("wss://ws.kraken.com/v2");
    client.run();
}
```

---

## 📈 Why Wirekrak?

- Designed for high-frequency trading  
- Low-latency, stable, high-throughput  
- Clean API and modular architecture  
- Works seamlessly with FlashStrike matching engine  

---

## 📜 License

MIT License

---

## 🤝 Contributions

PRs welcome.  
Developed for the Kraken Forge Hackathon.

