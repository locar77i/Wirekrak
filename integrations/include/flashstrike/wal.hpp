#pragma once

#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/constants.hpp"
#include "flashstrike/wal/segment/header.hpp"
#include "flashstrike/wal/segment/block.hpp"
#include "flashstrike/wal/segment/block_header.hpp"
#include "flashstrike/wal/utils.hpp"

    

// -----------------------------------------------------------------------------
// WAL BLOCK INTEGRITY DESIGN
// -----------------------------------------------------------------------------
// Context:
//   The current WAL only protects the segment header via a checksum. While this
//   guarantees header-level integrity, it leaves event regions vulnerable to
//   partial writes, bit flips, or silent disk corruption. For an ultra-low-
//   latency trading engine, we need stronger protection without degrading
//   throughput, cache efficiency, or event alignment.
//
// Recommended Design: Dual Checksum per WAL Block
//   - Keep RequestEvent structures 64B aligned (no per-event checksum).
//   - Group events into fixed-size blocks (typ. 32–64 events, ≈2–4 KB).
//     Each block is preceded by a small header that stores two checksums:
//
//       struct wal::segment::BlockHeader {
//           uint16_t event_count;       // actual valid events in this block
//           uint64_t block_checksum;    // checksum(events[]) — LOCAL integrity
//           uint64_t chained_checksum;  // checksum(events[] + prev_chain) — GLOBAL chain
//       };
//
//   - block_checksum detects isolated corruption in this block.
//   - chained_checksum links blocks together for full-segment determinism.
//
// Checksum Computation (example using XXH64 or CRC32C):
//     block_checksum   = XXH64(events, sizeof(RequestEvent)*N, /*seed=*/0);
//     chained_checksum = XXH64(events, sizeof(RequestEvent)*N, prev_chained);
//
//   The segment header may store the final chained_checksum of the last block
//   to enable cross-segment validation.
//
// Validation During Replay:
//   For each block:
//     1. Recompute local = XXH64(events, size, 0);
//     2. Recompute chain = XXH64(events, size, prev_chained);
//     3. If either mismatch → corruption detected.
//   If corruption occurs:
//     - In STRICT (deterministic) mode: stop replay immediately.
//     - In DIAGNOSTIC (best-effort) mode: skip or resync to next valid block.
//
// Benefits of Dual-Checksum Scheme:
//   - Detects any partial or mis-ordered block writes.
//   - Supports deterministic replay via chained checksum.
//   - Allows safe localized recovery using local checksum only.
//   - No per-event overhead; RequestEvent remains exactly 64B.
//   - Sequential checksum computation fits fully in L1/L2 cache.
//
// Performance Characteristics (typical modern CPU):
//   - 32–64 events/block → 2–4 KB contiguous region per checksum.
//   - CRC32C (hardware accelerated): <2 ns/event overhead.
//   - XXH64 (software): ~3–5 ns/event overhead.
//   - Header overhead: ~0.6 % of WAL size (16 B per 2–4 KB).
//
// Recovery Semantics:
//   - STRICT mode → stop on first mismatch (preserves deterministic replay).
//   - DIAGNOSTIC mode → attempt resync; recovered data is non-deterministic,
//     used only for forensic or operational inspection.
//
// Summary:
//   ┌─────────────────────────────┬────────────────────────────┐
//   │   Checksum Type             │ Purpose                    │
//   ├─────────────────────────────┼────────────────────────────┤
//   │ block_checksum (local)      │ Detect intra-block errors  │
//   │ chained_checksum (global)   │ Ensure order & continuity  │
//   └─────────────────────────────┴────────────────────────────┘
//
//   Recommended defaults:
//     BLOCK_SIZE        = 64 events (≈4 KB)
//     HASH_ALGORITHM    = CRC32C (HW) or XXH64 (SW fallback)
//     RECOVERY_MODE     = STRICT for engine replay
//
// Implementation Note:
//   Updates to wal::segment::Header includes the field `last_chained_checksum`
//   to anchor cross-segment validation.
//   Diagnostic tools may expose both checksums for corruption analytics.
//
// -----------------------------------------------------------------------------
