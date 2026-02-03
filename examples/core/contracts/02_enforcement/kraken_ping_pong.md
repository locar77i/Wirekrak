# Control Plane: Ping / Pong

This example demonstrates **Wirekrak Core's control-plane support** using
explicit Ping and Pong messages.

Control-plane messages are **independent of market data subscriptions** and
exist to provide observability, diagnostics, and heartbeat verification.

---

## Contract Demonstrated

- Control-plane messages (ping, pong, status) are independent of subscriptions
- Pong responses are delivered via a dedicated protocol callback
- No market data subscriptions are required
- Engine timestamps and local wall-clock time can be correlated
- Control-plane traffic does not affect subscription state

If a pong is received, it is because **the protocol explicitly sent a ping**.

---

## Scope and Constraints

This example intentionally does **not**:

- Subscribe to any market data channels
- Rely on implicit heartbeats
- Simulate failures or reconnects
- Depend on liveness timeouts

It focuses exclusively on **explicit control-plane interaction**.

---

## Execution Flow

1. A Kraken session is created with no subscriptions.
2. Protocol-level status messages are observed.
3. A single ping is sent using the control-plane API.
4. The corresponding pong is received and reported.
5. Round-trip time is measured:
   - Engine RTT (if provided by the exchange)
   - Local wall-clock RTT

The example terminates once the pong is observed.

---

## How to Run the Example

1. Run the program with a valid Kraken WebSocket URL.
2. Observe status messages emitted by the exchange.
3. Observe the pong response to the explicit ping request.
4. Review RTT measurements printed to stdout.

No additional interaction is required.

---

## Summary

> **Ping and pong are explicit, protocol-level control-plane messages.  
> They are observable, deterministic, and independent of subscriptions.**

This example exists to make that contract concrete and executable.
