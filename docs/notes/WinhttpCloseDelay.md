# WebSocket Close Delay on Windows (WinHTTP)

## Observed Behavior

When Wirekrak forces a reconnect due to a **heartbeat timeout**, you may observe
a **~10 second pause** before the WebSocket fully closes:

```
[WARN]  Heartbeat timeout (10012 ms). Forcing reconnect.
[DEBUG] [WS:API] Closing WebSocket ...
[DEBUG] WebSocket closed.
... ~10 seconds pause ...
[TRACE] [WS] WebSocket closed.
```

This behavior is **expected** on Windows and is not a Wirekrak bug.

---

## Root Cause

Wirekrak uses **WinHTTP WebSockets** with **Windows SChannel TLS**.

On Windows:

- `WinHttpWebSocketClose()` is **asynchronous**
- The OS waits for:
  - TLS shutdown
  - TCP FIN / timeout
  - Internal WinHTTP cleanup
- The final close notification is delivered **~10 seconds later**

This delay is imposed by the **Windows networking stack**, not by Wirekrak.

---

## Why Wirekrak Still Reconnects Correctly

Wirekrak **does not block** on the final close event:

- The streaming client:
  - Marks the connection as closed immediately
  - Schedules reconnection independently
- The delayed close notification is safely ignored
- No deadlock or resource leak occurs

This design ensures:
- Low-latency reconnect logic
- Deterministic behavior
- No dependency on OS timing

---

## Design Decision

Wirekrak intentionally:

- Avoids waiting for WinHTTP close completion
- Treats close as a **fire-and-forget** operation
- Uses internal state instead of OS callbacks for liveness

This is standard practice in professional Windows networking code.

---

## Summary

- The ~10s delay is **normal WinHTTP behavior**
- It is **OS-controlled**, not configurable
- Wirekrak handles it safely and correctly
- Reconnection logic is unaffected

---

## Reference

- Microsoft WinHTTP WebSocket documentation
- Windows SChannel TLS shutdown semantics

---

⬅️ [Back to README](../../README.md#platform-notes)
