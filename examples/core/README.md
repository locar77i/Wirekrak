# Wirekrak Core — Examples

This directory contains **runnable examples** that demonstrate the core
behavioral guarantees of **Wirekrak Core**.

These examples are not exchange SDKs, tutorials, or “happy path” demos.
They exist to make Wirekrak’s **design contracts observable**.

If you want to understand:
- what Wirekrak guarantees,
- what it deliberately does *not* assume,
- how responsibility is split between layers,
- why certain behaviors are enforced,

this is the correct entry point.

---

## Who These Examples Are For

These examples are intended for:

- Engineers integrating Wirekrak into production systems
- Users validating behavioral guarantees and responsibility boundaries
- Readers who want to understand *what Wirekrak enforces* versus *what it observes*
- Anyone reasoning about correctness, not convenience

These examples are **not intended as onboarding material**.

If you are new to Wirekrak, or want to see market data flowing with minimal setup,
start with the lightweight onboarding examples:

➡️ [Lite Examples — Getting Started](../lite/README.md)

---

## What these examples are

Wirekrak Core examples are:

- **Layer-driven**  
  Each example targets a specific architectural layer.

- **Behavior-first**  
  They demonstrate *what actually happens*, not what is expected or intended.

- **Observable by design**  
  Logs and telemetry are part of the lesson.

- **Strictly honest**  
  No guessing, no inferred health, no hidden recovery.

You are expected to:
- run the examples,
- observe the output,
- and reason about the results.

---

## What these examples are NOT

These examples are **not**:

- ❌ Trading bots  
- ❌ Exchange SDKs  
- ❌ Protocol abstractions  
- ❌ “Always-connected” demos  

They intentionally expose:
- forced reconnects,
- missing deliveries,
- strict lifecycle ordering,
- protocol responsibility boundaries.

If something feels uncomfortable,
it is probably correct.

---

## Example Progression

### Transport examples <a name="transport"></a>

Transport examples demonstrate **how data moves**, **how connections behave**,
and **what the system can observe** — without protocol assumptions.

They focus on:
- WebSocket behavior
- Connection lifecycle
- Transport vs delivery semantics
- Message framing and fragmentation
- Error and close ordering
- Liveness enforcement

These examples are intentionally **protocol-agnostic**.

Transport examples are organized following the Transport architecture described here:

➡️ [Transport Examples Overview](./transport/Overview.md)

From that document, you can navigate to:
- WebSocket transport behavior
- Connection layer semantics
- Related transport-specific examples

---

### Protocol examples

Protocol examples demonstrate **how responsibility shifts above transport**.

They focus on:
- Subscription logic
- Heartbeat strategies
- Exchange-specific semantics
- Protocol-driven liveness
- Correct interaction with the Connection layer

Protocols are responsible for **producing traffic**.
Wirekrak will not fabricate it.

Protocol examples are organized by protocol or exchange and build on top of
the Transport layer behavior demonstrated earlier.

---

## Recommended learning order

If you are new to Wirekrak Core:

1. Start with **Transport examples**
2. Read the Transport overview document
3. Observe how behavior is enforced
4. Move to **Protocol examples**
5. Compare responsibility boundaries

Wirekrak does not optimize for convenience.

It optimizes for correctness.

---

## Design philosophy reminder

Wirekrak Core follows one rule above all others:

> **Correctness over convenience**

These examples exist to make that rule visible.

If an example contradicts your assumptions,
trust the example.

---

⬅️ [Back to README](../../README.md#core-examples)
