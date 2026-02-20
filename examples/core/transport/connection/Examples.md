## 📚 Connection Examples & Learning Path

Wirekrak’s Connection examples are designed as a **progressive learning path**.  
Each example introduces **one core concept** and builds directly on the previous one.

If you are new to Wirekrak, **follow them in order**.

---

### 🟢 Example 0 — Minimal Connection <a name="minimal"></a>
*(Learning Step 1: Getting started)*

**Goal:** Learn the absolute minimum required to use a `Connection`.

- Open a WebSocket connection
- Drive it with `poll()`
- Observe connect and disconnect signals
- Close cleanly and deterministically
- Inspect connection and transport telemetry

➡️ [Minimal Connection](./00_minimal/README.md)

---

### 🟡 Example 1 — Message Shape & Fragmentation <a name="fragmentation"></a>
*(Learning Step 2: Observing the wire)*

**Goal:** Understand what *actually* happens on the WebSocket wire.

- Messages vs frames
- Fragmentation behavior
- Message size as an observed property
- Transport reports facts, not sender intent

➡️ [Message Shape & Fragmentation](./01_fragmentation/README.md)

---

### 🟠 Example 2 — Observation vs Consumption <a name="delivery"></a>
*(Learning Step 3: Observation ≠ consumption)*

**Goal:** Learn the boundary between transport observation and application consumption.

- Messages may arrive without being consumed
- Consumption requires explicit `peek_message()` + `release_message()`
- `RX messages ≠ messages_forwarded_total` is correct behavior
- Transport observation does not imply application interest

➡️ [Observation vs Consumption](./02_delivery/README.md)

---

### 🔴 Example 3 — Failure, Disconnect & Close Ordering <a name="lifecycle"></a>
*(Learning Step 4: Failure correctness)*

**Goal:** Understand Wirekrak’s deterministic lifecycle guarantees.

- Error-before-close ordering
- Exactly-once disconnect semantics
- Idempotent physical closure
- Retry scheduling driven by cause, not timing
- Observable and ordered failure propagation

➡️ [Failure, Disconnect & Close Ordering](./03_lifecycle/README.md)

---

### 🔵 Example 4 — Heartbeat & Liveness Responsibility <a name="liveness"></a>
*(Learning Step 5: Protocol responsibility)*

**Goal:** Understand Wirekrak’s strict liveness model and responsibility split.

- Silence is unhealthy
- Passive connections are intentionally recycled
- The Connection enforces health invariants
- Protocols must emit observable keepalive traffic
- `LivenessThreatened` enables reactive protocol intervention

➡️ [Heartbeat & Liveness Responsibility](./04_liveness/README.md)

---

### 🧭 How to use these examples

- Each example is **self-contained**
- Each includes:
  - Teaching comments
  - Runtime explanations
  - Telemetry interpretation guidance
- The intended loop is:

  **run → observe logs → inspect telemetry → reason about behavior**

Do not skim. These examples are designed to be *experienced*.

---

### 🧠 Design philosophy reinforced by the examples

- No guessing
- No silent behavior
- No hidden recovery
- Clear responsibility boundaries
- Deterministic lifecycle transitions
- Telemetry reflects **what happened**, not what was intended

---

⬅️ [Back to Transport Examples](../README.md#connection)
