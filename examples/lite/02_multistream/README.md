# Lite Examples — Multiple Streams

This example demonstrates how to consume multiple market data streams
(trades and order book updates) using a single Wirekrak Lite client.

The key takeaway is that the Lite mental model does not change:
- One client
- One polling loop
- Independent callbacks per stream

No additional coordination or concurrency primitives are required.

This example builds directly on the previous Lite examples and introduces
no new concepts beyond subscribing to more than one stream.
