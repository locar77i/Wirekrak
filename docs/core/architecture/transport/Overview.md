# Wirekrak Transport Architecture

This document describes the **transport layer architecture** used by Wirekrak.
It is designed to be **low-latency**, **testable**, and **fully decoupled**
from protocol and exchange-specific logic.

---

## Goals

- Zero dynamic polymorphism (no virtual functions)
- Compile-time validation via C++20 concepts
- Transport-agnostic connection abstraction
- Easy mocking for unit tests
- Windows-native TLS via WinHTTP (SChannel)

```Note:``` WinHTTP is the first production transport implementation, chosen for
its tight integration with Windows networking and native SChannel TLS support.

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
                    │   transport::Connection      │
                    │                              │
                    │  • liveness detection        │
                    │  • automatic reconnect       │
                    │  • heartbeat handling        │
                    │  • transport-level isolation │
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
                    │ transport::<implementation>  │
                    │                              │
                    │  • OS / library WebSocket    │
                    │  • TLS integration           │
                    └──────────────────────────────┘
```

---

## Core Concepts

```Key Idea:``` Concepts, Not Base Classes

WWirekrak avoids inheritance and instead uses **C++20 concepts** to define
compile-time contracts between layers.

---

### `transport::WebSocketConcept`

`WebSocketConcept` defines the generic **WebSocket transport contract**
required by the transport connection layer, independent of any platform
or library.

- connect / send / close
- callbacks for message and close events

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

This enables:
- real platform implementations
- fully mocked transports for testing
- future backends (Boost.Beast, ASIO, etc.)

---

## Reference Implementation <a name="winhttp"></a>

The first reference transport implementation is based on **WinHTTP**.

- `transport::winhttp::WebSocket` implements `WebSocketConcept`
- Uses Windows-native WebSocket support
- TLS is provided by Windows SChannel
- No OpenSSL or third-party networking stack required

Implementation-specific API abstraction details are documented separately:

➡️ [WinHTTP Transport Implementation](./winhttp/Transport.md)

---

## Why This Design?

- **Ultra-low latency**: no vtables, no hidden allocations
- **Concepts over base classes**
- **Zero-cost abstractions**
- **Safety**: compile-time errors instead of runtime failures
- **Testability**: mock WebSocket transports
- **Extensibility**: new transports without touching protocol code

---

## Testing

- `WebSocketConcept` enables full transport mocking
- Liveness, reconnection, and error paths are unit-tested
- No network access required for transport tests

---

## Summary

- Wirekrak uses **modern C++20 concepts** instead of inheritance
- Transport layer is **fully decoupled** from exchange logic
- Architecture is **implementation-agnostic**
- Platform-specific transports are isolated and replaceable
- Designed for **high-frequency streaming workloads**

---

⬅️ [Back to README](../../ARCHITECTURE.md#transport)
