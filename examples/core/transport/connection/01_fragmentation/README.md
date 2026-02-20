# Example 1 — Message Shape & Fragmentation

This example demonstrates how Wirekrak reports **WebSocket message shape**
based strictly on **observable wire behavior**, not sender intent or protocol
assumptions.

> **Core truth:**  
> Telemetry describes what *actually happened on the wire* — not what the
> exchange or application *meant* to send.

This example intentionally pulls all available messages from the connection
data-plane in order to observe their reconstructed size and fragmentation
characteristics.

---

## What this example proves

- Message size is an **observed outcome**, not a protocol promise
- WebSocket fragmentation is **visible and measurable**
- Transport telemetry reflects **assembly and framing**, not payload semantics
- Pulling (consumption) is separate from transport observation
- Wirekrak does not reinterpret or normalize message shape

---

## What this example is NOT

- ❌ It does not parse messages
- ❌ It does not assume schema correctness
- ❌ It does not reconstruct sender intent
- ❌ It does not “fix” fragmentation
- ❌ It does not automatically consume messages

This example is about **observation, availability, and explicit consumption** —
not interpretation.

---

## Scenario walkthrough

1. A WebSocket connection is opened
2. A subscription message is sent
3. The exchange emits messages of varying sizes
4. The application explicitly pulls messages via `peek_message()`
5. Wirekrak records:
   - Assembled message sizes
   - Fragment counts
   - Fragment totals
6. Telemetry is dumped for inspection

No assumptions are made about:

- Exchange framing strategy
- Compression
- Message boundaries
- Sender intent

---

## Key telemetry fields explained

### WebSocket RX messages

- Count of **fully assembled messages observed at the transport layer**
- Reflects what arrived on the wire
- Independent of whether the application consumes them

---

### Messages forwarded

- Incremented when the application calls `peek_message()`
- Represents explicit consumption of available messages
- Always ≤ WebSocket RX messages

---

### RX message bytes

- Distribution of assembled message sizes
- Includes:
  - min / max / avg
  - last observed size
- Reflects *post-assembly* payload size
- Observed fact, not a protocol guarantee

---

### Fragments/msg

- Average number of wire fragments per message
- Values:
  - `1` → single-frame messages
  - `>1` → fragmented messages
- Indicates sender or transport framing behavior

---

### RX fragments

- Total number of fragment frames observed
- Helps answer:
  - “Was fragmentation used at all?”
  - “How frequently?”

---

## Why this matters

Many systems assume:

- “One message = one frame”
- “Message size equals payload size”
- “Fragmentation is rare or irrelevant”

These assumptions are often wrong.

Wirekrak makes fragmentation **visible**, measurable, and undeniable.

---

## Design contract demonstrated

- Transport reports **facts**
- Connection exposes **availability**
- Applications explicitly **consume**
- Message shape is an **observable property**
- Fragmentation is **not hidden**
- Telemetry matches wire reality exactly

---

## How to interpret results

| Observation | Meaning |
|------------|--------|
| Fragments/msg = 1 | Messages arrived unfragmented |
| Fragments/msg > 1 | Messages were split across frames |
| RX fragments = 0 | No fragmentation observed |
| Wide RX size range | Variable message payloads |
| Forwarded < RX | Application did not pull all available messages |

---

## TL;DR

If the wire fragments messages,  
**Wirekrak tells you.**

If messages vary in size,  
**Wirekrak records it.**

If you do not pull messages,  
**they are not consumed.**

Nothing is guessed.  
Nothing is normalized.

Wirekrak reports reality.  
It does not hide it.

---

⬅️ [Back to Connection Examples](../Examples.md#fragmentation)
