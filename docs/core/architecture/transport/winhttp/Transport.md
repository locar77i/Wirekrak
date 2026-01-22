# WinHTTP Transport Implementation

This document describes the **WinHTTP-based WebSocket transport** used by Wirekrak
as the first production transport implementation.

The WinHTTP transport provides a **Windows-native, low-latency WebSocket backend**
that integrates directly with **SChannel TLS**, without relying on external
networking libraries or OpenSSL.

---

## Overview

The WinHTTP transport is a concrete implementation of the following transport-layer contracts:

- `transport::WebSocketConcept`
- `transport::winhttp::ApiConcept`

It is responsible **only** for:
- establishing and maintaining a WebSocket at the OS level
- sending and receiving raw WebSocket frames
- surfacing events via callbacks

All higher-level concerns such as:
- connection lifecycle
- liveness detection
- reconnection logic

are handled **above** this layer by `core::transport::Connection`.

---

## Design Goals

- Use **Windows-native networking APIs**
- Leverage **SChannel TLS** for secure connections
- Avoid OpenSSL and third-party networking stacks
- Maintain **zero dynamic polymorphism**
- Enable **full unit testing** via API abstraction
- Keep platform-specific code isolated

---

## Layering

```
core::transport::Connection
        ▲
        │ WebSocketConcept
        │
transport::winhttp::WebSocket
        ▲
        │ ApiConcept
        │
transport::winhttp::RealApi
```

---

## transport::winhttp::WebSocket

`transport::winhttp::WebSocket` is the concrete WebSocket type used by Wirekrak
on Windows.

It implements `transport::WebSocketConcept` and provides:

- `connect(host, port, path)`
- `send(message)`
- `close()`
- callback registration for:
  - incoming messages
  - socket closure

It does **not**:
- attempt reconnection
- track heartbeats
- interpret message contents
- manage protocol state

---

## transport::winhttp::ApiConcept

To enable testing and zero-cost abstraction, WinHTTP API calls are abstracted
behind a C++20 concept.

This allows injection of:
- real WinHTTP APIs
- fake APIs for unit testing

with no runtime overhead.

---

## TLS and Security

- TLS is provided by **Windows SChannel**
- Certificate validation is handled by the OS
- No OpenSSL dependency

---

## Error Handling Model

- WinHTTP errors surface as transport-level failures
- Recovery and retries are handled exclusively by
  `core::transport::Connection`

---

## Testing Strategy

- Mock implementations of `ApiConcept`
- Deterministic error injection
- No network access required

---

## Summary

- WinHTTP is the first concrete transport implementation in Wirekrak
- Fully isolated behind C++20 concepts
- Platform-specific code is strictly contained
- Higher layers remain transport-agnostic

---

⬅️ [Back to Transport Overview](../Overview.md#winhttp)
