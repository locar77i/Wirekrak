#pragma once

#include <cstdint>


namespace flashstrike {
namespace wal {

// -----------------------------------------------------------------------------
// WAL RECOVERY MODES
// -----------------------------------------------------------------------------
// Determines how the WAL recovery manager behaves when a checksum mismatch
// or corruption is detected.
//
// STRICT      → Deterministic replay. Stop immediately on first error.
//                Used by the trading engine and audit systems.
// DIAGNOSTIC  → Non-deterministic (best-effort) replay. Attempt to resync
//                after a corruption to salvage readable data for analysis.
//                NEVER used to rebuild live trading state.
//
enum class RecoveryMode : uint8_t {
    STRICT = 0,
    DIAGNOSTIC = 1
};


// Status codes for WAL operations
enum class Status : uint8_t {
    OK = 0,
    DIRECTORY_NOT_FOUND,
    SEGMENT_NOT_FOUND,
    ITEM_NOT_FOUND,
    OPEN_FAILED,
    CLOSE_FAILED,
    FILE_ALREADY_EXISTS,
    FILE_NOT_DELETED,
    WRITE_FAILED,
    WRITE_HEADER_FAILED,
    READ_FAILED,
    READ_HEADER_FAILED,
    FSYNC_FAILED,
    MSYNC_FAILED,
    ROTATE_FAILED,
    HEADER_CHECKSUM_MISMATCH,
    BLOCK_CHECKSUM_MISMATCH,
    CHAINED_CHECKSUM_MISMATCH,
    SEGMENT_CORRUPTED,
    SEGMENT_POSSIBLY_CORRUPTED
};
// to_string for Status
static inline const char* to_string(Status status) noexcept{
    switch (status) {
        case Status::OK: return "Ok";
        case Status::DIRECTORY_NOT_FOUND: return "Directory Not Found";
        case Status::SEGMENT_NOT_FOUND: return "Segment Not Found";
        case Status::ITEM_NOT_FOUND: return "Item Not Found";
        case Status::OPEN_FAILED: return "Open Failed";
        case Status::CLOSE_FAILED: return "Close Failed";
        case Status::FILE_ALREADY_EXISTS: return "File Already Exists";
        case Status::FILE_NOT_DELETED: return "File Not Deleted";
        case Status::WRITE_FAILED: return "Write Failed";
        case Status::WRITE_HEADER_FAILED: return "Write Header Failed";
        case Status::READ_FAILED: return "Read Failed";
        case Status::READ_HEADER_FAILED: return "Read Header Failed";
        case Status::FSYNC_FAILED: return "Fsync Failed";
        case Status::MSYNC_FAILED: return "Msync Failed";
        case Status::ROTATE_FAILED: return "Rotate Failed";
        case Status::HEADER_CHECKSUM_MISMATCH: return "Header Checksum Mismatch";
        case Status::BLOCK_CHECKSUM_MISMATCH: return "Block Checksum Mismatch";
        case Status::CHAINED_CHECKSUM_MISMATCH: return "Chained Checksum Mismatch";
        case Status::SEGMENT_CORRUPTED: return "Segment Corrupted";
        case Status::SEGMENT_POSSIBLY_CORRUPTED: return "Segment Possibly Corrupted";
        default: return "Unknown Status";
    }
}


// Possible consistency issues detected during verify_consistency()
enum class Consistency {
    Ok,                          // Everything is consistent
    TooManyHotSegments,          // > max segments .wal files
    TooManyColdSegments,         // > max compressed segments .lz4 files
    ColdListMismatch,            // compressed segments != files on disk
    EmptyFileDetected,           // Found a zero-length file
    UnknownError                 // Catch-all
};
// Human-readable names for Consistency
static inline const char* to_string(Consistency status) noexcept {
    switch (status) {
        case Consistency::Ok: return "Ok";
        case Consistency::TooManyHotSegments: return "Too many hot segments";
        case Consistency::TooManyColdSegments: return "Too many cold segments";
        case Consistency::ColdListMismatch: return "Cold segment list mismatch";
        case Consistency::EmptyFileDetected: return "Empty file detected";
        case Consistency::UnknownError: return "Unknown consistency error";
        default: return "Unknown Consistency type";
    }
}

} // namespace wal
} // namespace flashstrike
