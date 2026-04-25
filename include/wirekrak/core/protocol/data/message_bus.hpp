#pragma once

/*
===============================================================================
MessageBus - Zero-overhead typed message routing
===============================================================================

The MessageBus is a compile-time indexed container of message queues.

It provides:
  • Type-safe push/pop by message type
  • Zero runtime overhead (compile-time dispatch)
  • Fixed-capacity lock-free SPSC queues per message type

This implementation uses:
  lcr::lockfree::spsc_queue<T, N>

Capacity is configurable via ring_traits<T>.

===============================================================================
*/

#include <tuple>
#include <utility>
#include <type_traits>

#include "wirekrak/core/meta/type_list.hpp"
#include "wirekrak/core/config/protocol.hpp"
#include "lcr/lockfree/spsc_queue.hpp"

namespace wirekrak::core::protocol::data {

// ============================================================================
// Ring traits (customizable per message type)
// ============================================================================
template<class T>
struct ring_traits {
    static constexpr std::size_t capacity =
        config::protocol::DEFAULT_RING_CAPACITY;
};

// ============================================================================
// MessageBus
// ============================================================================
template<class MessageList>
class MessageBus;

// ----------------------------------------------------------------------------
// Specialization for meta::type_list
// ----------------------------------------------------------------------------
template<class... Messages>
class MessageBus<meta::type_list<Messages...>> {

    // Ensure all message types are unique at compile time
    static_assert(meta::type_list_assert_unique_v<meta::type_list<Messages...>>, "MessageBus requires unique message types");

public:
    using message_list = meta::type_list<Messages...>;
    static constexpr std::size_t size = sizeof...(Messages);

private:
    // One SPSC ring per message type
    std::tuple<
        lcr::lockfree::spsc_queue<
            Messages,
            ring_traits<Messages>::capacity
        >...
    > queues_;

public:
    MessageBus() noexcept = default;

    // =========================================================================
    // PUSH
    // =========================================================================
    template<class Message>
    [[nodiscard]]
    inline bool push(Message&& msg) noexcept {
        using T = std::decay_t<Message>;
        static_assert(meta::type_list_contains_v<T, message_list>, "Message type not registered in MessageBus");
        // Push the message into the appropriate queue
        auto& q = queue_<T>();
        return q.push(std::forward<Message>(msg));
    }

    // =========================================================================
    // POP
    // =========================================================================
    template<class Message>
    [[nodiscard]]
    inline bool pop(Message& out) noexcept {
        static_assert(meta::type_list_contains_v<Message, message_list>, "Message type not registered in MessageBus");
        // Pop the message from the appropriate queue
        auto& q = queue_<Message>();
        return q.pop(out);
    }

    // =========================================================================
    // DRAIN
    // =========================================================================
    template<class Message, class F>
    [[nodiscard]]
    inline std::size_t drain(F&& fn) noexcept {
        static_assert(meta::type_list_contains_v<Message, message_list>, "Message type not registered in MessageBus");
        auto& q = queue_<Message>();
        std::size_t count = 0;
        Message msg;
        while (q.pop(msg)) {
            fn(msg);
            ++count;
        }
        return count;
    }

    template<class Message, class F>
    [[nodiscard]]
    inline std::size_t drain_n(std::size_t max, F&& fn) noexcept {
        static_assert(meta::type_list_contains_v<Message, message_list>, "Message type not registered in MessageBus");
        auto& q = queue_<Message>();
        std::size_t count = 0;
        Message msg;
        while (count < max && q.pop(msg)) {
            fn(msg);
            ++count;
        }

        return count;
    }

    // =========================================================================
    // EMPTY
    // =========================================================================
    template<class Message>
    [[nodiscard]]
    inline bool empty() const noexcept {
        static_assert(meta::type_list_contains_v<Message, message_list>, "Message type not registered in MessageBus");
        // Check if the appropriate queue is empty
        const auto& q = queue_<Message>();
        return q.empty();
    }

    // =========================================================================
    // CLEAR
    // =========================================================================
    template<class Message>
    inline void clear() noexcept {
        Message tmp;
        while (pop<Message>(tmp)) {}
    }

    // =========================================================================
    // GLOBAL EMPTY CHECK
    // =========================================================================
    [[nodiscard]]
    inline bool empty() const noexcept {
        return empty_impl_(std::make_index_sequence<size>{});
    }

private:
    // =========================================================================
    // INTERNAL: Queue access
    // =========================================================================
    template<class Message>
    inline auto& queue_() noexcept {
        constexpr std::size_t index =
            meta::type_list_index_v<Message, message_list>;
        return std::get<index>(queues_);
    }

    template<class Message>
    inline const auto& queue_() const noexcept {
        constexpr std::size_t index =
            meta::type_list_index_v<Message, message_list>;
        return std::get<index>(queues_);
    }

    // =========================================================================
    // INTERNAL: empty() fold
    // =========================================================================
    template<std::size_t... I>
    [[nodiscard]]
    inline bool empty_impl_(std::index_sequence<I...>) const noexcept {
        return (std::get<I>(queues_).empty() && ...);
    }
};

} // namespace wirekrak::core::protocol::data
