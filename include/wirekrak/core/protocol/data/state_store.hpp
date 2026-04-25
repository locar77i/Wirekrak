#pragma once

/*
===============================================================================
StateStore - Zero-overhead typed state storage
===============================================================================

The StateStore is a compile-time indexed container of single-value slots.

It complements MessageBus:

  • MessageBus → streams (queues)
  • StateStore → snapshots (latest value)

------------------------------------------------------------------------------
Key properties
------------------------------------------------------------------------------

• Zero runtime overhead
    - No virtual dispatch
    - No dynamic allocation
    - Compile-time indexing via meta::type_list

• Type-safe
    - State is accessed by type
    - No casting or tagging

• Snapshot semantics
    - Only the latest value is stored
    - New writes overwrite previous value

------------------------------------------------------------------------------
Usage example
------------------------------------------------------------------------------

using States = meta::type_list<
    Pong,
    Status
>;

StateStore<States> store;

store.set(Pong{...});

if (auto* p = store.get<Pong>()) {
    // use latest pong
}

------------------------------------------------------------------------------
Design
------------------------------------------------------------------------------

StateStore is implemented as:

    std::tuple<optional<T1>, optional<T2>, ...>

Each type maps to a slot via compile-time index lookup.

------------------------------------------------------------------------------
*/

#include <tuple>
#include <type_traits>

#include "wirekrak/core/meta/type_list.hpp"
#include "lcr/optional.hpp"

namespace wirekrak::core::protocol::data {

// ============================================================================
// StateStore
// ============================================================================
template<class StateList>
class StateStore;

// ----------------------------------------------------------------------------
// Specialization for meta::type_list
// ----------------------------------------------------------------------------
template<class... States>
class StateStore<meta::type_list<States...>> {

    // Ensure all state types are unique at compile time
    static_assert(meta::type_list_assert_unique_v<meta::type_list<States...>>, "StateStore requires unique state types");

public:
    using state_list = meta::type_list<States...>;
    static constexpr std::size_t size = sizeof...(States);

private:
    // One optional slot per state type
    std::tuple<lcr::optional<States>...> slots_;

public:
    StateStore() noexcept = default;

    // =========================================================================
    // SET (overwrite latest value)
    // =========================================================================
    template<class State>
    inline void set(State&& value) noexcept {
        using T = std::decay_t<State>;
        // Ensure the state type is registered in the StateStore
        static_assert(meta::type_list_contains_v<T, state_list>, "State type not registered in StateStore");
        slot_<T>() = std::forward<State>(value);
    }

    // =========================================================================
    // GET (pointer access)
    // =========================================================================
    template<class State>
    [[nodiscard]]
    inline State* get() noexcept {
        // Ensure the state type is registered in the StateStore
        static_assert(meta::type_list_contains_v<State, state_list>, "State type not registered in StateStore");
        // Return pointer to the value if it exists, or nullptr if not set
        auto& s = slot_<State>();
        return s.has_value() ? &s.value() : nullptr;
    }

    template<class State>
    [[nodiscard]]
    inline const State* get() const noexcept {
        // Ensure the state type is registered in the StateStore
        static_assert(meta::type_list_contains_v<State, state_list>, "State type not registered in StateStore");
        // Return pointer to the value if it exists, or nullptr if not set
        const auto& s = slot_<State>();
        return s.has_value() ? &s.value() : nullptr;
    }

    // =========================================================================
    // HAS
    // =========================================================================
    template<class State>
    [[nodiscard]]
    inline bool has() const noexcept {
        // Ensure the state type is registered in the StateStore
        static_assert(meta::type_list_contains_v<State, state_list>, "State type not registered in StateStore");
        // Check if the slot has a value
        return slot_<State>().has_value();
    }

    // =========================================================================
    // CLEAR (per state)
    // =========================================================================
    template<class State>
    inline void clear() noexcept {
        // Ensure the state type is registered in the StateStore
        static_assert(meta::type_list_contains_v<State, state_list>, "State type not registered in StateStore");
        // Reset the slot to empty
        slot_<State>().reset();
    }

    // =========================================================================
    // CLEAR ALL
    // =========================================================================
    inline void clear_all() noexcept {
        clear_all_impl_(std::make_index_sequence<size>{});
    }

private:
    // =========================================================================
    // INTERNAL: slot access
    // =========================================================================
    template<class State>
    inline auto& slot_() noexcept {
        constexpr std::size_t index = meta::type_list_index_v<State, state_list>;
        return std::get<index>(slots_);
    }

    template<class State>
    inline const auto& slot_() const noexcept {
        constexpr std::size_t index = meta::type_list_index_v<State, state_list>;
        return std::get<index>(slots_);
    }

    // =========================================================================
    // INTERNAL: clear_all
    // =========================================================================
    template<std::size_t... I>
    inline void clear_all_impl_(std::index_sequence<I...>) noexcept {
        (std::get<I>(slots_).reset(), ...);
    }
};

} // namespace wirekrak::core::protocol::data
