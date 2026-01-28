# Transport Layer — Overview

The **Transport layer** in Wirekrak Core is responsible for **observing reality**.

It answers questions like:
- What actually arrived on the wire?
- When did a connection really close?
- Did anything happen — or did it merely look connected?
- In what order did errors, closes, and retries occur?

Transport does **not** interpret protocol meaning.
It reports **facts**, enforces **invariants**, and exposes **telemetry**.

The fastest way to understand this layer is through its examples.

---

## How to learn the Transport layer

Wirekrak transport behavior is intentionally strict and explicit.
Reading the code alone is not enough.

You are expected to:
1. Run the transport examples
2. Observe logs and telemetry
3. Compare behavior to expectations
4. Internalize what the layer guarantees — and what it refuses to assume

Transport examples are organized by sub-layer.

---

## Transport / WebSocket *(future)* <a name="websocket"></a>

This section will contain examples focused on **raw WebSocket behavior**,
before any connection-level policy is applied.

Planned areas of exploration include:
- Frame vs message semantics
- Fragmentation and reassembly
- Backpressure and receive behavior
- Transport-level error reporting
- Platform-specific WebSocket differences

These examples will answer:
> “What did the WebSocket actually do?”

This section will grow as transport coverage expands.

---

## Transport / Connection <a name="connection"></a>

Connection examples focus on the **Connection layer**, which sits above the raw
WebSocket transport and enforces **connection-level invariants**.

This is where Wirekrak becomes opinionated.

These examples demonstrate:
- Logical connection lifecycle
- Transport vs delivery semantics
- Message forwarding rules
- Error and close ordering
- Exactly-once disconnect guarantees
- Deterministic retry behavior
- Liveness enforcement
- Explicit protocol responsibility

They answer questions like:
- Why did the connection reconnect?
- Why was nothing forwarded even though messages arrived?
- Why was the socket closed even though TCP was still open?
- Why does liveness require protocol traffic?

If you want to understand **why connections behave the way they do**, start here.

➡️ [Connection Examples](./connection/Examples.md)

---

## Design rule reminder

The Transport layer follows one uncompromising rule:

> **Report reality. Enforce invariants. Do not guess.**

If something disconnects, stalls, or retries:
- it is observable,
- it is counted,
- and it happened for a reason you can trace.

Transport examples exist to make those reasons visible.

---

⬅️ [Back to Wirekrak Core Examples](../README.md#transport)
