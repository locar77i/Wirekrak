#pragma once

#include <cstdint>


namespace flashstrike {
namespace wal {


// Tune this based on empirical latency testing
constexpr size_t WAL_BLOCK_EVENTS = 256;  // 256 × 64 B = 16 KB data section
constexpr size_t MIN_BLOCKS = 16;         // segment_size_ = 64 KB minimum
constexpr size_t MAX_BLOCKS = 512 * 1024; // segment_size_ = 2 GB maximum
constexpr uint16_t WAL_MAGIC = 0x4653;    // "FS" (little-endian)
constexpr uint8_t WAL_VERSION = 0x01;     // WAL format version 1

constexpr size_t WAL_RING_BUFFER_SIZE = 128; // SPSC ring buffer size for recovery worker
constexpr size_t WAL_MIN_HOT_SEGMENTS = 2;  // Min number of hot (uncompressed) segments to keep
constexpr size_t WAL_MAX_HOT_SEGMENTS = WAL_RING_BUFFER_SIZE; // Max number of hot (uncompressed) segments to keep
constexpr size_t WAL_MIN_COLD_SEGMENTS = WAL_MIN_HOT_SEGMENTS * 2; // Min number of cold (compressed) segments to keep
constexpr size_t WAL_MAX_COLD_SEGMENTS = WAL_MAX_HOT_SEGMENTS * 4; // Max number of cold (compressed) segments to keep
constexpr size_t MAX_PRELOADED_SEGMENTS = 4; // Max number of preloaded segments in recovery worker ring buffer. The goal is to hide I/O latency without overcommitting RAM.

constexpr size_t WAL_PERSIST_RING_BUFFER_SIZE = WAL_MAX_HOT_SEGMENTS;
constexpr size_t WAL_HOT_RING_BUFFER_SIZE = WAL_MAX_HOT_SEGMENTS;
constexpr size_t WAL_COLD_RING_BUFFER_SIZE = WAL_MAX_COLD_SEGMENTS;

} // namespace wal
} // namespace flashstrike
