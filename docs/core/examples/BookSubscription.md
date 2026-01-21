# Wirekrak Book Subscription Example (Single & Multi-Symbol)

This example demonstrates how to subscribe to **Kraken order book updates**
using Wirekrak, supporting **one or multiple symbols**, clean shutdown via
**Ctrl+C**, and rejection handling.

---

## Features

- Subscribe to **one or multiple symbols**
- Receive real-time **order book updates**
- Demonstrates **rejection handling** (double subscription)
- Uses **CLI11** for robust CLI parsing
- Runs until interrupted with **Ctrl+C**
- Clean unsubscribe on exit

---

## Usage

```bash
wirekrak_book_updates [OPTIONS]
```

### Options

| Option | Description |
|------|------------|
| `-s, --symbol SYMBOL` | Trading symbol(s), repeatable |
| `--url URL` | Kraken WebSocket URL |
| `--double-sub` | Subscribe twice to trigger rejection |
| `-l, --log-level LEVEL` | trace \| debug \| info \| warn \| error |
| `-h, --help` | Show help |

---

## Examples

### Single symbol
```bash
wirekrak_book_updates -s BTC/USD
```

### Multiple symbols
```bash
wirekrak_book_updates -s BTC/USD -s ETH/USD -s SOL/USD
```

### Demonstrate rejection handling
```bash
wirekrak_book_updates -s BTC/USD --double-sub
```

---

## Runtime Behavior

- The application runs indefinitely.
- Incoming book updates are printed to stdout.
- Press **Ctrl+C** to:
  1. Unsubscribe cleanly
  2. Drain pending messages
  3. Exit gracefully

---

## Notes

- Uses Kraken WebSocket API v2
- TLS handled via Windows SChannel
- Designed for **low-latency streaming**

---

⬅️ [Back to README](../../../README.md#examples)
