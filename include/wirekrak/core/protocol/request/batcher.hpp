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
class Batcher {
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
    // Should emit next batch?
    // ---------------------------------------------------------------------

    [[nodiscard]]
    inline bool should_emit() noexcept {
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
    // Add request
    // ---------------------------------------------------------------------

    template <typename RequestT>
    requires DynamicJsonWritable<RequestT>
    [[nodiscard]]
    bool add_request(const RequestT& req) noexcept {
        auto* slot = queue_.acquire_producer_slot();
        if (!slot) [[unlikely]] {
            return false;
        }
        const std::size_t size = req.write_json(slot->data());
        if (size > slot->capacity()) [[unlikely]] {
            return false;
        }
        slot->set_size(size);
        queue_.commit_producer_slot();
        return true;
    }

    // ---------------------------------------------------------------------
    // Get next serialized message
    // ---------------------------------------------------------------------

    [[nodiscard]]
    bool next(std::string_view& out) noexcept {
        auto* slot = queue_.peek_consumer_slot();
        if (!slot) [[unlikely]] {
            return false;
        }
        out = std::string_view(slot->data(), slot->size());
        queue_.release_consumer_slot();
        return true;
    }

private:

    std::size_t poll_counter_{0};
    lcr::local::ring<buffer_type, QueueCapacity> queue_;

};

} // namespace wirekrak::core::protocol::request
