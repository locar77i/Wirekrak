# Wirekrak Transport Architecture

This document describes the **transport layer architecture** used by Wirekrak.
It is designed to be **low-latency**, **testable**, and **fully decoupled**
from protocol and exchange-specific logic.

---

## Goals

- Zero dynamic polymorphism (no virtual functions)
- Compile-time validation via C++20 concepts
- Transport-agnostic streaming client
- Easy mocking for unit tests
- Windows-native TLS via WinHTTP (SChannel)

---

## High-Level Architecture

```
                    ┌──────────────────────────────┐
                    │   Exchange Protocol Session  │
                    │   (Kraken, etc.)             │
                    │                              │
                    │  • subscriptions             │
                    │  • message routing           │
                    │  • schema parsing            │
                    └──────────────▲───────────────┘
                                   │
                                   │ decoded messages
                                   │
                    ┌──────────────┴───────────────┐
                    │   stream::Client             │
                    │                              │
                    │  • liveness detection        │
                    │  • automatic reconnect       │
                    │  • heartbeat handling        │
                    │  • backpressure isolation    │
                    └──────────────▲───────────────┘
                                   │
                                   │ raw text frames
                                   │
                    ┌──────────────┴───────────────┐
                    │ transport::WebSocketConcept  │
                    │                              │
                    │  • connect / send / close    │
                    │  • callbacks                 │
                    │                              │
                    │ (concept, no inheritance)    │
                    └──────────────▲───────────────┘
                                   │
                                   │ platform calls
                                   │
                    ┌──────────────┴───────────────┐
                    │ transport::winhttp           │
                    │                              │
                    │  • WinHTTP WebSocket         │
                    │  • Windows SChannel TLS      │
                    │                              │
                    │ winhttp::RealApi             │
                    └──────────────────────────────┘
```

---

## Core Concepts

```Key Idea:``` Concepts, Not Base Classes

Wirekrak avoids inheritance and instead uses **C++20 concepts**.

### `transport::WebSocketConcept`

WebSocketConcept defines the generic **WebSocket transport contract** required by streaming clients,
independent of the underlying platform or implementation:

- connect / send / close
- callbacks for message / close / error

```cpp
template<class WS>
concept WebSocketConcept =
    requires(
        WS ws,
        const std::string& host,
        const std::string& port,
        const std::string& path,
        const std::string& msg
    ) {
        { ws.connect(host, port, path) } -> std::same_as<bool>;
        { ws.send(msg) } -> std::same_as<bool>;
        { ws.close() } -> std::same_as<void>;
    };
```

This allows:
- Real implementations (WinHTTP)
- Mock transports (unit tests)
- Future transports (Boost.Beast, ASIO, etc.)

---

### `transport::winhttp::ApiConcept`

The Windows API itself is also abstracted using a concept.

ApiConcept defines the minimal WinHTTP WebSocket API surface required by the transport layer.
This enables dependency injection, testing, and zero-overhead abstraction via C++20 concepts.

```cpp
template<class Api>
concept ApiConcept =
    requires(Api api, HINTERNET ws, void* buf, DWORD size) {
        { api.websocket_send(ws, WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, buf, size) }
            -> std::same_as<DWORD>;
    };
```


Benefits:
- No direct WinHTTP calls inside core logic
- Testable with fake APIs
- Header-only, zero overhead

---

## Real Implementation

This keeps **platform-specific code isolated**.
- `winhttp::RealWinApi` implements `ApiConcept`
- `winhttp::WebSocket` implements `WebSocketConcept`
- Uses Windows SChannel (no OpenSSL)

```cpp
struct RealApi {
    DWORD websocket_send(HINTERNET ws, WINHTTP_WEB_SOCKET_BUFFER_TYPE type,
                         void* buffer, DWORD size) noexcept {
        return WinHttpWebSocketSend(ws, type, buffer, size);
    }
};
```

---

## Why This Design?

- **Ultra-low latency**: no vtables, no heap allocation
- **Concepts over base classes**
- **Zero-cost abstractions**
- **Safety**: compile-time errors instead of runtime failures
- **Testability**: mock WebSocket + mock API
- **Extensibility**: new transports without touching protocol code

---

## Testing

- WebSocketConcept enables full transport mocking
- Liveness, reconnection, and error paths are unit-tested
- No network access required for tests

---

## Summary

- Wirekrak uses **modern C++20 concepts** instead of inheritance
- Transport layer is **fully decoupled** from exchange logic
- Design is **zero-cost**, **header-only**, and **test-friendly**
- Built for **high-frequency streaming workloads**

---

⬅️ [Back to README](../../../ARCHITECTURE.md#transport)
