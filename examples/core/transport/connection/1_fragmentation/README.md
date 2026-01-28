# Example 1 — Message Shape & Fragmentation

This example demonstrates how Wirekrak reports **WebSocket message shape**
based strictly on **observable wire behavior**, not sender intent or protocol
assumptions.

> **Core truth:**  
> Telemetry describes what *actually happened on the wire* — not what the
> exchange or application *meant* to send.

---

## What this example proves

- Message size is an **observed outcome**, not a protocol promise
- WebSocket fragmentation is **visible and measurable**
- Transport telemetry reflects **assembly and framing**, not payload semantics
- Wirekrak does not reinterpret or normalize message shape

---

## What this example is NOT

- ❌ It does not parse messages
- ❌ It does not assume schema correctness
- ❌ It does not reconstruct sender intent
- ❌ It does not “fix” fragmentation

This example is about **observation, not interpretation**.

---

## Scenario walkthrough

1. A WebSocket connection is opened
2. A subscription message is sent
3. The exchange emits messages of varying sizes
4. Wirekrak records:
   - Message sizes
   - Fragment counts
   - Fragment totals
5. Telemetry is dumped for inspection

No assumptions are made about:
- Exchange framing strategy
- Compression
- Message boundaries

---

## Key telemetry fields explained

### RX messages

- Count of **fully assembled messages**
- Represents what the application receives
- Independent of fragmentation

---

### RX message bytes

- Distribution of assembled message sizes
- Includes:
  - min / max / avg
  - last observed size
- Reflects *post-assembly* payload size

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

- Transport reports **facts**, not interpretations
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

---

## TL;DR

If the wire fragments messages,  
**Wirekrak tells you.**

If messages vary in size,  
**Wirekrak records it.**

Nothing is guessed.  
Nothing is normalized.

Wirekrak reports reality.  
It does not hide it.
