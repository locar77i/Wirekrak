# Lite Examples — Getting Started with Wirekrak

This directory contains **minimal, progressive onboarding examples** for the
Wirekrak Lite client.

The goal of the Lite examples is to help you:
- Connect to an exchange quickly
- Subscribe to market data streams
- Consume data using a stable, exchange-agnostic API
- Build confidence using Wirekrak’s Lite façade

These examples intentionally hide protocol, transport, and recovery mechanics.
You do not need to understand internal correctness to use the Lite API effectively.

---

## Who These Examples Are For

- First-time users of Wirekrak
- Engineers evaluating the Lite API
- Anyone who wants to see market data flowing in minutes

These examples focus on **how to use Lite**, not how it works internally.
If you are looking to explore Wirekrak’s internal guarantees, lifecycle behavior, or
correctness enforcement, see:

➡️ [Core Examples — Getting Started](../core/README.md)  

---

## How to Use These Examples

Each folder in this directory:
- Is self-contained
- Can be run independently
- Builds conceptually on the previous examples

You can start at `00_quickstart` and progress in order, or jump directly to the
example that matches your needs.

---

## Example Progression

### `00_quickstart`
**Minimal connection and subscription**

- Connect to an exchange
- Subscribe to a single market data stream
- Consume data via callbacks
- Exit cleanly

Start here if this is your first time using Wirekrak.

---

### `01_subscriptions`
**Configurable subscriptions**

- Subscribe to multiple symbols
- Configure subscriptions at runtime
- Handle errors explicitly
- Cleanly unsubscribe and shut down

These examples show how Lite is typically used in real programs.

---

### `02_multistream`
**Multiple streams, same mental model**

- Consume different market data streams at the same time
- Use a single client and polling loop
- Handle each stream independently

No additional coordination or concurrency primitives are required.

---

### `03_lifecycle`
**Client lifecycle and shutdown**

- Run a long-lived Lite client
- Shut down cleanly using Ctrl+C
- Explicitly manage the client lifecycle

No subscription management or delivery guarantees are demonstrated.

---

## What These Examples Do Not Cover

The Lite examples intentionally do **not** define or expose:

- Internal invariants or correctness proofs
- Recovery or replay semantics
- Delivery completeness guarantees
- Backpressure or lifecycle modeling
- Protocol- or transport-specific behavior

These topics are covered by the executable examples in the `core` layer:

➡️ [Core Examples — Getting Started](../core/README.md)  

---

## Design Philosophy

Wirekrak scales in complexity with the user.

The Lite examples prioritize:
- Fast feedback
- Low cognitive overhead
- A stable and disciplined API contract

Internal correctness, determinism, and recovery are enforced by Core, but are
**not part of the Lite contract**.

---

⬅️ [Back to README](../../README.md#lite-examples)
