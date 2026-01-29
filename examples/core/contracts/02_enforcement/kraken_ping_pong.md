# Control Plane — Ping / Pong

This example demonstrates Wirekrak Core’s **control-plane support**.

Ping, pong, and status messages are delivered independently of market data
subscriptions and can be used for heartbeat verification, latency measurement,
and operational monitoring.

---

## Contract Demonstrated

- Control-plane messages are independent of subscriptions
- Pong responses are delivered via a dedicated callback
- Engine timestamps and local wall-clock time can be correlated
- No market data channels are required

---

## Summary

> **Wirekrak Core exposes a deterministic control plane separate from data
> subscriptions, suitable for monitoring and liveness checks.**
