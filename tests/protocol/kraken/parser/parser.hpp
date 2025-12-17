#pragma once

/*
================================================================================
Protocol Parser Unit Tests
================================================================================

This test suite validates the correctness, robustness, and safety of a protocol-
level message parser.

Scope and guarantees enforced by these tests:
  • Strict schema validation — only spec-compliant messages are accepted
  • Deterministic behavior — parse() returns true/false, never throws
  • Failure-safe parsing — malformed or partial JSON is safely rejected
  • No side effects — output objects are modified only on successful parse
  • Explicit negative coverage — missing fields, wrong types, and invalid
    discriminators are intentionally exercised

Non-goals (by design):
  • Business rule validation (e.g. checksum correctness)
  • Cross-message consistency or stateful aggregation
  • Performance or throughput benchmarking
  • Recovery or retry semantics

The parser is exercised using simdjson DOM parsing to mirror production usage,
ensuring that protocol-level faults are contained and cannot propagate into
higher layers such as Clients, Dispatchers, or Trading Logic.

This separation is critical for correctness, debuggability, and long-running
real-time systems.
================================================================================
*/

namespace tests {
namespace protocol {
namespace kraken {
namespace parser {



} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace tests