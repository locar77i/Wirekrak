#pragma once

// Stl headers
#include <cstdint>
#include <atomic>
#include <string>
#include <type_traits>
#include <filesystem>
#include <system_error>
#include <cassert>
// POSIX
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// Project headers

#include "flashstrike/constants.hpp"
#include "flashstrike/wal/recorder/telemetry/meta.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/log/Logger.hpp"


namespace flashstrike {
namespace wal {
namespace recorder {

// ============================================================================
//  struct MetaState
//  ----------------------------------------------------------------------------
//  Compact, trivially copyable representation of the current WAL metadata state.
//
//  This structure encodes the essential Write-Ahead Log (WAL) position:
//      • last_segment_index  — index of the last active WAL segment
//      • last_offset         — byte offset within that segment
//      • last_event_id       — global identifier of the last appended event
//
//  The structure is fixed at 16 bytes to ensure atomic persistence and
//  memory efficiency. It is trivially copyable, allowing it to be safely
//  written to or read from disk as a raw binary blob without serialization.
//
//  Usage Context:
//  --------------
//  • Used by `MetaStore` to store and persist WAL progress.
//  • Updated atomically through packed 64-bit operations for segment index
//    and offset, and separately for event ID.
//  • Designed for direct memory-mapped or low-level file I/O.
//
//  Thread Safety:
//  --------------
//  • The struct itself is a passive data container and contains no synchronization.
//  • When embedded in `MetaStore`, it becomes effectively thread-safe due to the
//    atomic access patterns applied at the `MetaStore` level.
//
//  Performance Notes:
//  ------------------
//  • Fixed-size (16 bytes) layout enables aligned, lock-free atomic updates.
//  • Trivially copyable → zero overhead for persistence or inter-thread passing.
//  • No padding or dynamic data.
//
//  Invariants:
//  -----------
//  • sizeof(MetaState) == 16
//  • std::is_trivially_copyable_v<MetaState> == true
//  ----------------------------------------------------------------------------

struct MetaState {
    uint32_t last_segment_index{0};
    uint32_t last_offset{0};
    uint64_t last_event_id{INVALID_EVENT_ID};
};
static_assert(sizeof(MetaState) == 16, "MetaState must be 16 bytes");
static_assert(std::is_trivially_copyable_v<MetaState>);


// ============================================================================
//  class MetaStore
//  ----------------------------------------------------------------------------
//  Thread-safe, lock-free metadata manager for the low-latency WAL subsystem.
//
//  This class maintains and persists the logical WAL state, consisting of:
//      • last_segment_index  — index of the last written WAL segment
//      • last_offset         — current byte offset within that segment
//      • last_event_id       — globally increasing identifier of the last event
//
//  Data is stored compactly in a 16-byte structure (`MetaState`) and persisted
//  to disk as a small binary file in the WAL directory.
//
//  Key Characteristics:
//  --------------------
//  • Hot-path optimized — `update()` performs atomic, lock-free updates.
//  • Atomic persistence — on-disk state is updated via a temp file + rename,
//    ensuring crash consistency without partial writes.
//  • No dynamic allocations in the hot path.
//  • Designed for integration with an asynchronous flush worker.
//
//  Thread Safety:
//  --------------
//  • Fully thread-safe.
//  • Multiple threads may concurrently call `update()` and `get_state()`.
//  • `flush_to_disk()` is also safe to call concurrently, though typically it
//    should be invoked by a single background flusher thread.
//  • All shared fields (`state_`, `last_event_id_`, `dirty_`) are atomic.
//
//  Typical Usage:
//  --------------
//      MetaStore meta("/var/lib/app", "wal.meta");
//      if (!meta.load()) {
//          // initialize metadata from scratch
//      }
//
//      // Hot path (no locks, no syscalls)
//      meta.update(segment_idx, offset, event_id);
//
//      // Background persistence
//      if (meta.is_dirty()) {
//          meta.flush_to_disk();
//      }
//
//  Performance Notes:
//  ------------------
//  • Updates (`update()`) are non-blocking and extremely fast (~a few ns).
//  • Flushing involves disk I/O and fsync, so it should not occur in the hot path.
//  • Uses atomic rename to guarantee durability and consistency across crashes.
//
//  Dependencies:
//  -------------
//  • C++17 or later
//  • POSIX file APIs (open/read/write/fdatasync/rename)
//  • Optional metrics integration via ENABLE_FS1_METRICS
//  ----------------------------------------------------------------------------
class MetaStore {
public:
    explicit MetaStore(const std::string& dir, const std::string& fname, telemetry::MetaStore& metrics)
        : meta_path_(dir + "/" + fname)
        , metrics_updater_(metrics)
    {}

    // Hot-path update, lock-free
    inline void update(uint32_t last_segment_index, uint32_t last_offset, uint64_t last_event_id) noexcept {
        uint64_t packed = pack_state_(last_segment_index, last_offset);
        state_.store(packed, std::memory_order_release);
        last_event_id_.store(last_event_id, std::memory_order_release);
        dirty_.exchange(true, std::memory_order_acq_rel);
    }

    [[nodiscard]] inline bool flush_to_disk() noexcept {
        return flush_to_disk_();
    }

    // Load meta on startup
    [[nodiscard]] inline bool load() noexcept {
        if (!std::filesystem::exists(meta_path_)) return false;

        int fd = ::open(meta_path_.c_str(), O_RDONLY);
        if (fd < 0) return false;
        // Read packed state
        uint64_t packed = 0;
        ssize_t n = ::read(fd, &packed, sizeof(packed));
        if (n != sizeof(packed)) {
            ::close(fd);
            return false;
        }
        // Read last_event_id
        uint64_t last_event_id = 0;
        n = ::read(fd, &last_event_id, sizeof(last_event_id));
        ::close(fd);
        if (n != sizeof(last_event_id)) return false;
        state_.store(packed, std::memory_order_release);
        last_event_id_.store(last_event_id, std::memory_order_release);
        return true;
    }

    // Accessors
    inline MetaState get_state() const noexcept {
        uint64_t packed = state_.load(std::memory_order_acquire);
        return MetaState{
            static_cast<uint32_t>(packed >> 32),
            static_cast<uint32_t>(packed & 0xFFFFFFFF),
            last_event_id_.load(std::memory_order_acquire)
        };
    }

    inline bool is_dirty() const noexcept {
        return dirty_.load(std::memory_order_acquire);
    }

    inline const std::string& filepath() const noexcept { return meta_path_; }

private:
    std::string meta_path_;
    std::atomic<bool> dirty_{false};
    std::atomic<uint64_t> state_{0};
    std::atomic<uint64_t> last_event_id_{0};

    telemetry::MetaUpdater metrics_updater_;


    // ------------------------------------------------------------------------
    // Helpers
    static inline uint64_t pack_state_(uint32_t segment_index, uint32_t offset) noexcept {
        return (static_cast<uint64_t>(segment_index) << 32) | offset;
    }

    // Flush meta to disk
    [[nodiscard]] inline bool flush_to_disk_() noexcept {
#ifdef ENABLE_FS1_METRICS
        uint64_t start_ns = monotonic_clock::instance().now_ns();
#endif // #ifdef ENABLE_FS1_METRICS
        if (!dirty_.exchange(false, std::memory_order_acq_rel)) return true; // nothing to do
        WK_DEBUG("[->]   Flushing WAL meta to disk: " << meta_path_);
        uint64_t packed = state_.load(std::memory_order_acquire);
        uint64_t last_event_id = last_event_id_.load(std::memory_order_acquire);
        std::string tmp_path = meta_path_ + ".tmp";
        int fd = ::open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) return false;
        // Write packed state
        ssize_t n = ::write(fd, &packed, sizeof(packed));
        if (n != sizeof(packed)) {
            ::close(fd);
            ::unlink(tmp_path.c_str());
            return false;
        }
        // Write last_event_id
        n = ::write(fd, &last_event_id, sizeof(last_event_id));
        if (n != sizeof(last_event_id)) {
            ::close(fd);
            ::unlink(tmp_path.c_str());
            return false;
        }
        // Ensure durability of data in the file
        if (::fdatasync(fd) != 0) {
            ::close(fd);
            ::unlink(tmp_path.c_str());
            return false;
        }
        // Close file
        ::close(fd);
        // Atomic rename
        std::error_code ec;
        std::filesystem::rename(tmp_path, meta_path_, ec);
        if (ec) return false;
        // Ensure durability of the renamed file in the directory
        int dir_fd = ::open(std::filesystem::path(meta_path_).parent_path().c_str(), O_DIRECTORY);
        if (dir_fd >= 0) {
            ::fdatasync(dir_fd);
            ::close(dir_fd);
        }
        WK_DEBUG("[->]   WAL meta flushed successfully: " << meta_path_ << "(segment_index=" << (packed >> 32) << ", offset=" << (packed & 0xFFFFFFFF) << ", last_event_id=" << last_event_id << ")");
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_async_meta_flush_completed(start_ns);
#endif // #ifdef ENABLE_FS1_METRICS
        return true;
    }
};


} // namespace recorder
} // namespace wal
} // namespace flashstrike
