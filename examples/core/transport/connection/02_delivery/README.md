# Example 2 - Connection vs Transport Semantics

This example demonstrates a **fundamental design boundary in Wirekrak**:

> Receiving data on the wire is not the same as consuming data in the
> application.

Wirekrak makes this distinction explicit, observable, and measurable.

There are no callbacks. There is no implicit delivery. Messages are only
consumed when explicitly pulled.

---

## What this example proves

-   The WebSocket **transport** may receive messages continuously.

-   The **Connection layer** exposes messages via a pull-based
    data-plane.

-   Messages are only counted as forwarded when the application calls:

        peek_message() + release_message()

-   Telemetry correctly reflects the separation between arrival and
    consumption.

In short:

> **Observation ≠ Consumption**

---

## What this example is NOT

-   ❌ It is not a subscription tutorial\
-   ❌ It does not guarantee message consumption\
-   ❌ It does not auto-install callbacks\
-   ❌ It does not infer user intent

If you expect messages to be consumed automatically just because they
arrive on the wire, this example exists to correct that assumption.

---

## Phases explained

### Phase A - Transport receives, application does NOT consume

-   A WebSocket connection is opened.
-   A valid subscription is sent.
-   The application does NOT call `peek_message()`.
-   The transport receives messages.
-   The Connection exposes them, but they are not consumed.

Expected outcome:

-   `RX messages > 0`
-   `Messages forwarded == 0`

This is correct behavior.

---

### Phase B - Explicit consumption enabled

-   The application begins calling `peek_message()`.
-   Messages are now:
    -   Observed on the wire
    -   Made available by the Connection
    -   Explicitly consumed by user code

Expected outcome:

-   `Messages forwarded` starts increasing
-   Consumed messages appear in logs

Consumption is intentional and observable.

---

## What to watch for in the telemetry

| Metric | Meaning |
|------|--------|
| RX messages | Messages reassembled from frames and received from the network |
| Messages forwarded | Messages explicitly consumed via `peek_message()` |
| RX ≥ Forwarded | Always true, by design |

If these numbers were equal by default, the Connection layer would be
leaking policy.

---

## Design contract demonstrated

-   Transport reports **facts**
-   Connection exposes **availability**
-   Applications perform **consumption**
-   Telemetry reflects observable reality

Nothing is hidden. Nothing is inferred. Nothing is auto-delivered.

---

## Why this matters

This separation enables:

-   Passive observation
-   Late-binding consumers
-   Deterministic replay
-   Precise telemetry
-   Testable behavior

And prevents:

-   Accidental message handling
-   Implicit side effects
-   Double counting
-   Hidden coupling

---

## TL;DR

If no one calls `peek_message()`,\
**messages are not consumed - by design.**

Wirekrak separates transport from intent.

---

Wirekrak enforces correctness -\
it does not hide responsibility.

---

⬅️ [Back to Connection Examples](../Examples.md#delivery)
