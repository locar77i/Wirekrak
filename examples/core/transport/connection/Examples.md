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
- Observe connect, message, and disconnect events
- Close cleanly and deterministically

➡️ [Minimal Connection](./00_minimal/README.md)

---

### 🟡 Example 1 — Message Shape & Fragmentation <a name="fragmentation"></a>
*(Learning Step 2: Observing the wire)*

**Goal:** Understand what *actually* happens on the WebSocket wire.

- Messages vs frames
- Fragmentation behavior
- Message size as an observed property
- Why sender intent does not matter

➡️ [Message Shape & Fragmentation](./01_fragmentation/README.md)

---

### 🟠 Example 2 — Transport vs Delivery Semantics <a name="delivery"></a>
*(Learning Step 3: Observation ≠ delivery)*

**Goal:** Learn the boundary between transport and application logic.

- Messages may arrive without being delivered
- Delivery requires an explicit message callback
- `messages_rx_total ≠ messages_forwarded_total` is correct behavior

➡️ [Transport vs Delivery Semantics](./02_delivery/README.md)

---

### 🔴 Example 3 — Error & Close Lifecycle <a name="lifecycle"></a>
*(Learning Step 4: Failure correctness)*

**Goal:** Understand Wirekrak’s deterministic lifecycle guarantees.

- Error-before-close ordering
- Exactly-once disconnect semantics
- No double counting of lifecycle events
- Retry behavior driven by cause, not timing

➡️ [Error & Close Lifecycle](./03_lifecycle/README.md)

---

### 🔵 Example 4 — Heartbeat-Driven Liveness <a name="liveness"></a>
*(Learning Step 5: Protocol responsibility)*

**Goal:** Understand Wirekrak’s strict liveness model.

- Silence is unhealthy
- Passive connections are recycled
- The Connection enforces liveness
- Protocols must emit keepalive traffic
- **Liveness warning hooks enable proactive protocol action**

➡️ [Heartbeat-Driven Liveness](./04_liveness/README.md)

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
- Telemetry reflects **what happened**, not what was intended

---

⬅️ [Back to Transport Overview](../Overview.md#connection)
