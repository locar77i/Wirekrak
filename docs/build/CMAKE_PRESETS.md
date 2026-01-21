# Wirekrak Build Presets

Wirekrak uses CMake Presets to make architectural intent explicit and reproducible.
Presets define *what* is built, not just *how* it is built.

## Core-only (Infrastructure / ULL)

### ninja-core-only
- Builds Wirekrak Core only (header-only)
- No Lite, no tests, no examples, no telemetry
- Intended for ultra-low-latency and infrastructure use

## Development Builds

### ninja-debug / ninja-release
- Core + tests
- No Lite
- Used for protocol and infrastructure development

## SDK / Examples

### ninja-*-examples
- Enables Wirekrak Lite
- Builds example applications
- Intended for SDK users and demonstrations

## Experimental Integrations

### ninja-experimental
- Enables Lite and experimental integrations
- Not built by default
- May depend on external or prototype code

## Benchmarks

### ninja-*-bench / ninja-bench-minimal
- Core-only benchmarks
- No Lite
- Telemetry level explicitly controlled
- Used for performance analysis and regression testing

## Design Rule

Core-only presets must never accidentally build Lite or higher-level components.
All non-core functionality is opt-in by design.

---

⬅️ [Back to README](../../README.md#cmake-presets)
