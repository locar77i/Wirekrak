# Control Plane: Ping / Pong

This example demonstrates **Wirekrak Core's protocol-level control-plane support**
using explicit **Ping** and **Pong** messages.

Control-plane traffic is **orthogonal to market data subscriptions** and exists
solely to provide **observability, diagnostics, and latency insight**.  
It does not alter transport behavior, subscription state, or replay semantics.

---

## Contract Demonstrated

- Control-plane messages (ping, pong, status) are **protocol-level traffic**
- Control-plane traffic is **independent of subscriptions**
- Pong responses are delivered via a **dedicated protocol callback**
- No market data subscriptions are required
- Engine timestamps and local wall-clock time can be correlated
- Control-plane traffic does **not** affect subscription or replay state
- All progress is driven explicitly via `poll()`

If a pong is observed, it is because **the protocol explicitly replied to a ping**.
Nothing is inferred or synthesized by the Core.

---

## Scope and Constraints

This example intentionally does **not**:

- Subscribe to any market data channels
- Assume or rely on implicit heartbeats
- Trigger or simulate reconnects
- Bypass or override transport liveness enforcement
- Measure reconnect or retry behavior

This example focuses exclusively on **explicit control-plane interaction** and
observable protocol facts.

---

## Execution Flow

1. A Kraken session is created with **no subscriptions**.
2. Protocol-level status messages are observed.
3. A single ping is sent using the control-plane API.
4. The corresponding pong is received and surfaced.
5. Round-trip time is measured and printed:
   - **Engine RTT** (if provided by the exchange)
   - **Local wall-clock RTT**

The example terminates immediately after the pong is observed.

---

## How to Run the Example

1. Run the program with a valid Kraken WebSocket URL.
2. Observe protocol status messages emitted by the exchange.
3. Observe the pong response to the explicit ping request.
4. Review RTT measurements printed to stdout.

No subscriptions, retries, or additional interaction are required.

---

## Design Notes

- Control-plane traffic is **owned by the protocol**, not the transport.
- Sending a ping does **not** reset or bypass transport liveness rules.
- The Connection never sends traffic on its own.
- All behavior is deterministic and poll-driven.

---

## Summary

> **Ping and pong are explicit, protocol-level control-plane messages.  
> They are observable facts, not inferred health signals.  
> They are deterministic, poll-driven, and independent of subscriptions.**

This example exists to make that contract explicit, testable, and unambiguous.
