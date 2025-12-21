# WireKrak Trade Subscription Example (Single & Multi-Symbol)

This example demonstrates how to subscribe to **Kraken trade events** using WireKrak,
supporting **one or multiple symbols**, optional **snapshot mode**, and clean shutdown
via **Ctrl+C**.

---

## Features

- Subscribe to **one or multiple symbols**
- Optional **trade snapshot** on subscription
- Demonstrates **rejection handling** (double subscription)
- Uses **CLI11** for professional CLI parsing
- Runs until interrupted with **Ctrl+C**
- Clean unsubscribe on exit

---

## Usage

```bash
wirekrak_trades [OPTIONS]
```

### Options

| Option | Description |
|------|------------|
| `-s, --symbol SYMBOL` | Trading symbol (repeatable) |
| `--url URL` | Kraken WebSocket URL |
| `--snapshot` | Request initial trade snapshot |
| `--double-sub` | Subscribe twice to trigger rejection |
| `-l, --log-level LEVEL` | trace \| debug \| info \| warn \| error |
| `-h, --help` | Show help |

---

## Examples

### Single symbol
```bash
wirekrak_trades -s BTC/USD
```

### Multiple symbols
```bash
wirekrak_trades -s BTC/USD -s ETH/USD -s SOL/USD
```

### Snapshot enabled
```bash
wirekrak_trades -s BTC/USD --snapshot
```

### Demonstrate rejection handling
```bash
wirekrak_trades -s BTC/USD --double-sub
```

---

## Runtime Behavior

- The application runs indefinitely.
- Incoming trade events are printed to stdout.
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

⬅️ [Back to README](../../README.md#examples)
