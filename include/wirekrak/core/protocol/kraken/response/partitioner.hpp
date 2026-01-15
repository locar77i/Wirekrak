#pragma once

#include <unordered_map>
#include <vector>
#include <span>

#include "wirekrak/core/protocol/kraken/response/traits.hpp"


namespace wirekrak::core::protocol::kraken::response {

/*
===============================================================================
Response Partitioner (Core Infrastructure)
===============================================================================

The Partitioner is a reusable, allocation-stable component that decomposes a
protocol Response into symbol-scoped ResponseView objects suitable for
deterministic routing and dispatch.

Key properties:
  - Generic over ResponseT via traits<ResponseT>
  - Header-only and fully inlineable
  - Zero-copy: never copies protocol messages
  - Allocation-free after warm-up (capacity reuse)
  - Produces non-owning ResponseView instances

Design intent:
  - Preserve protocol semantics (e.g. snapshot vs update)
  - Enable efficient per-symbol dispatch without modifying the dispatcher
  - Centralize response decomposition logic in one place
  - Serve as a stable Core v1 extension pattern for new Kraken channels

Lifetime & usage rules:
  - ResponseView objects are valid only during synchronous dispatch
  - The Partitioner must be reused via reset(), not reconstructed per message
  - Partitioner instances are not thread-safe and are intended to be owned
    by a single client / event loop

Extension:
  - Supporting a new Response type requires defining
    traits<ResponseT>
  - No runtime polymorphism or hooks are involved

===============================================================================
*/
template<class ResponseT>
class Partitioner {
    using traits = traits<ResponseT>;
    using message_type = typename traits::message_type;
    using view_type    = typename traits::view_type;

public:
    Partitioner() = default;
    Partitioner(const Partitioner&) = delete;
    Partitioner& operator=(const Partitioner&) = delete;

    inline void reset(const ResponseT& response) noexcept {
        response_ = &response;
        classify_();
    }

    [[nodiscard]]
    inline const std::vector<view_type>& views() const noexcept {
        return views_;
    }

private:
    const ResponseT* response_ = nullptr;

    std::unordered_map<Symbol, std::vector<const message_type*>> buckets_;

    std::vector<view_type> views_;

private:
    inline void classify_() noexcept {
        for (auto& [_, vec] : buckets_) {
            vec.clear();
        }
        views_.clear();

        for (const auto& msg : traits::messages(*response_)) {
            buckets_[traits::symbol_of(msg)].push_back(&msg);
        }

        views_.reserve(buckets_.size());

        for (auto& [symbol, vec] : buckets_) {
            views_.push_back(
                traits::make_view(
                    symbol,
                    traits::payload_type(*response_),
                    std::span<const message_type* const>(
                        vec.data(),
                        vec.size()
                    )
                )
            );
        }
    }
};

} // namespace wirekrak::core::protocol::kraken::response