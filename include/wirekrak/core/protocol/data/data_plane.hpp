#pragma once

#include <utility>
#include <type_traits>

#include "wirekrak/core/protocol/data/message_bus.hpp"
#include "wirekrak/core/protocol/data/state_store.hpp"
#include "wirekrak/core/meta/type_list.hpp"

namespace wirekrak::core::protocol::data {

template<
    class MessageList,
    class StateList
>
struct DataPlane {
    MessageBus<MessageList> messages;
    StateStore<StateList>   states;

    // =========================================================================
    // MESSAGE API
    // =========================================================================

    template<class Msg>
    [[nodiscard]]
    inline bool push(Msg&& msg) noexcept {
        return messages.push(std::forward<Msg>(msg));
    }

    template<class Msg>
    [[nodiscard]]
    inline bool try_pop(Msg& msg) noexcept {
        return messages.pop(msg);
    }

    template<class Msg, class F>
    inline std::size_t drain(F&& fn) noexcept {
        return messages.template drain<Msg>(std::forward<F>(fn));
    }

    template<class F>
    inline std::size_t drain_all(F&& fn) noexcept {
        return drain_all_impl_(std::forward<F>(fn), MessageList{});
    }

    template<class Msg>
    [[nodiscard]]
    inline bool empty() const noexcept {
        return messages.template empty<Msg>();
    }

    // =========================================================================
    // STATE API
    // =========================================================================

    template<class State>
    inline void set(State&& s) noexcept {
        states.set(std::forward<State>(s));
    }

    template<class State>
    [[nodiscard]]
    inline State* get() noexcept {
        return states.template get<State>();
    }

    template<class State>
    [[nodiscard]]
    inline const State* get() const noexcept {
        return states.template get<State>();
    }

    // =========================================================================
    // GLOBAL EMPTY (messages only)
    // =========================================================================
    [[nodiscard]]
    inline bool empty() const noexcept {
        return messages.empty();
    }

private:
    // =========================================================================
    // INTERNAL: drain_all implementation
    // =========================================================================
    template<class F, class... Msgs>
    inline std::size_t drain_all_impl_(F&& fn, meta::type_list<Msgs...>) noexcept {
        std::size_t total = 0;

        (void(
            total += messages.template drain<Msgs>(fn)
        ), ...);

        return total;
    }
};

} // namespace wirekrak::core::protocol::data
