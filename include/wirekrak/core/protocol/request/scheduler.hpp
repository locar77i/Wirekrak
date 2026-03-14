#pragma once

#include <string_view>
#include <algorithm>

#include "wirekrak/core/config/protocol.hpp"
#include "wirekrak/core/policy/protocol/batching.hpp"
#include "wirekrak/core/protocol/concept/json_writable.hpp"
#include "lcr/local/raw_buffer.hpp"
#include "lcr/local/ring.hpp"

namespace wirekrak::core::protocol::request {

template<
    typename Policy,
    std::size_t QueueCapacity = config::protocol::TX_BATCH_QUEUE_CAPACITY,
    std::size_t MaxPayload    = config::protocol::TX_BATCH_BUFFER_CAPACITY
>
class Scheduler {
public:

    using buffer_type = lcr::local::raw_buffer<MaxPayload>;

public:

    // ---------------------------------------------------------------------
    // Poll hook
    // ---------------------------------------------------------------------

    inline void poll() noexcept {
        ++poll_counter_;
    }

    // ---------------------------------------------------------------------

    [[nodiscard]]
    inline bool idle() const noexcept {
        return queue_.empty();
    }

    // ---------------------------------------------------------------------
    // Should send next batch?
    // ---------------------------------------------------------------------

    [[nodiscard]]
    inline bool should_send() noexcept {
        if constexpr (Policy::mode == policy::protocol::BatchingMode::Paced) {
            if (poll_counter_ < Policy::emit_interval) {
                return false;
            }
            poll_counter_ = 0;
            return true;
        }
        return false;
    }

    // ---------------------------------------------------------------------
    // Enqueue batch request
    // ---------------------------------------------------------------------

    template <typename RequestT>
    requires DynamicJsonWritable<RequestT>
    [[nodiscard]]
    bool enqueue(const RequestT& req) noexcept {
        auto* slot = queue_.acquire_producer_slot();
        if (!slot) [[unlikely]] {
            return false;
        }
        const std::size_t size = req.write_json(slot->data());
        if (size > slot->capacity()) [[unlikely]] {
            queue_.discard_producer_slot();
            return false;
        }
        slot->set_size(size);
        queue_.commit_producer_slot();
        return true;
    }

    // ---------------------------------------------------------------------
    // Dequeue batch request with peek/release semantics (for emission)
    // ---------------------------------------------------------------------

   [[nodiscard]]
    bool peek(std::string_view& out) noexcept {
        auto* slot = queue_.peek_consumer_slot();
        if (!slot) [[unlikely]] {
            return false;
        }

        out = std::string_view(slot->data(), slot->size());
        return true;
    }

    void release() noexcept {
        queue_.release_consumer_slot();
    }

private:

    std::size_t poll_counter_{0};
    lcr::local::ring<buffer_type, QueueCapacity> queue_;

};



struct NullScheduler {
    inline void poll() noexcept {}
    [[nodiscard]] inline bool idle() const noexcept { return true; }
    [[nodiscard]] inline bool should_send() noexcept { return false; }

    template<typename T>
    [[nodiscard]] bool enqueue(const T&) noexcept { return true; }

    [[nodiscard]] bool peek(std::string_view&) noexcept { return false; }
    void release() noexcept {}
};

} // namespace wirekrak::core::protocol::request
