# Wirekrak Telemetry Level Policy

**Status:** Stable  
**Applies to:** All Wirekrak components  
**Audience:** Infrastructure engineers, maintainers, contributors

---

## 1. Purpose

This document defines **telemetry levels** in Wirekrak and establishes **strict rules** governing:

- what metrics are allowed at each level
- how they are collected
- their performance and semantic guarantees
- how levels compose and evolve

The goal is to ensure telemetry remains:
- **honest**
- **low overhead**
- **predictable**
- **non-invasive**

---

## 2. Core Principles (Non-Negotiable)

All telemetry in Wirekrak MUST adhere to the following principles:

1. Telemetry must not change behavior
2. Telemetry must be removable at compile time
3. Telemetry must not introduce policy
4. Telemetry must not own clocks unless explicitly allowed
5. Telemetry must reflect observable reality, not inferred intent

Violations of these principles are considered **design errors**, not implementation bugs.

---

## 3. Telemetry Levels Overview

Telemetry is divided into **three cumulative levels**:

| Level | Name | Purpose |
|------|------|---------|
| L1 | Mechanical | Cheap, always-safe observability |
| L2 | Diagnostic | Deeper insight with controlled cost |
| L3 | Analytical | Heavy, exploratory, non-production |

### Hierarchy Rule

Higher levels imply lower levels:

- L2 ⇒ L1  
- L3 ⇒ L2 ⇒ L1  

This hierarchy is enforced at build configuration time.

---

## 4. Telemetry Level 1 (L1) — Mechanical Telemetry

### Intent

L1 telemetry captures **mechanical facts** about system behavior.

It answers:
- How much work happened?
- What was observed on the wire?
- How long did a known operation take (if explicitly measured)?

L1 telemetry is suitable for:
- production builds
- latency-sensitive paths
- always-on instrumentation

### Allowed Metrics

- Counters (bytes RX/TX, messages RX/TX, errors)
- Size / pressure metrics (message assembly size, queue depth)
- Simple per-event samplers (fragment count, callback duration)

### Forbidden in L1

- Owning clocks
- Computing rates
- Allocations
- Locks
- Histograms
- Policy decisions
- Control flow changes

### Performance Contract

- Zero cost when disabled
- Atomic operations only when enabled
- No branches in hot paths
- No syscalls
- No heap interaction

---

## 5. Telemetry Level 2 (L2) — Diagnostic Telemetry

### Intent

L2 telemetry supports **debugging and incident response**.

It answers:
- Why is this slow?
- Where is pressure accumulating?
- Which paths dominate execution cost?

### Allowed in L2

- Controlled clocks
- Scoped timers
- Limited histograms
- Aggregated latency distributions
- Retry and backoff statistics

L2 telemetry must remain compile-time gated and explicitly documented.

---

## 6. Telemetry Level 3 (L3) — Analytical Telemetry

### Intent

L3 telemetry is intended for:
- profiling
- research
- offline analysis
- experimental features

### Allowed in L3

- Dynamic allocation
- Detailed histograms
- Tracing and spans
- Correlation IDs
- Debug-only state capture

L3 telemetry is not production-safe by default.

---

## 7. Build-Time Configuration

Telemetry levels are enabled via compile-time flags:

- WIREKRAK_ENABLE_TELEMETRY_L1
- WIREKRAK_ENABLE_TELEMETRY_L2
- WIREKRAK_ENABLE_TELEMETRY_L3

Rules:
- Flags apply only to Wirekrak targets
- Flags are PUBLIC so headers see them
- Level hierarchy is enforced automatically

---

## 8. Evolution & Versioning

- Telemetry structs are versioned, not mutated
- Additive changes require a new version
- Removing or reinterpreting metrics within a version is forbidden

---

## 9. Summary

Wirekrak telemetry is deliberately conservative.

- Level 1 is mechanical, cheap, and always safe
- Level 2 is diagnostic and opt-in
- Level 3 is analytical and experimental

Telemetry must observe reality without influencing it, and must always be removable without altering correctness.

---

⬅️ [Back to README](../ARCHITECTURE.md#telemetry)
