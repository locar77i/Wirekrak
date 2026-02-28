#pragma once

#include <vector>
#include <string>
#include <variant>
#include <utility>
#include <cassert>

#include "wirekrak/core/transport/error.hpp"

namespace wirekrak::core::transport::test {

/*
===============================================================================
 MockWebSocketScript
===============================================================================

A deterministic, reusable script for driving MockWebSocket behavior in unit tests.

The script is a linear sequence of transport-level events (connect results,
incoming messages, errors, and close notifications). Each call to `step()`
executes exactly one scripted action.

Design principles:
- No threads, no timing assumptions
- Fully deterministic execution
- Explicit transport semantics
- Suitable for testing retry, close ordering, and error handling
===============================================================================
*/

class MockWebSocketScript {
public:
    // --- Script actions -----------------------------------------------------

    struct Connect {
        Error result{Error::None};
    };

    struct Message {
        std::string payload;
    };

    struct ErrorEvent {
        Error error{Error::TransportFailure};
    };

    struct Close {
    };

    using Action = std::variant<Connect, Message, ErrorEvent, Close>;

public:
    MockWebSocketScript() = default;

    // Append actions ---------------------------------------------------------

    inline MockWebSocketScript& connect_ok() {
        actions_.emplace_back(Connect{Error::None});
        return *this;
    }

    inline MockWebSocketScript& connect_fail(Error err) {
        actions_.emplace_back(Connect{err});
        return *this;
    }

    inline MockWebSocketScript& message(std::string msg) {
        actions_.emplace_back(Message{std::move(msg)});
        return *this;
    }

    inline MockWebSocketScript& error(Error err) {
        actions_.emplace_back(ErrorEvent{err});
        return *this;
    }

    inline MockWebSocketScript& close() {
        actions_.emplace_back(Close{});
        return *this;
    }

    // Execution --------------------------------------------------------------

    template <typename WS>
    inline void step(WS* ws) {
        assert(index_ < actions_.size() && "MockWebSocketScript exhausted");

        const auto& action = actions_[index_];

        // If no transport exists, only Connect actions are allowed
        if (!ws) {
            if (auto* c = std::get_if<Connect>(&action)) {
                WS::set_next_connect_result(c->result);
                ++index_;
                return;
            }
            assert(false && "Non-connect action requires active transport");
        }

        std::visit([&](auto&& action) {
            execute_(*ws, action);
        }, actions_[index_++]);
    }

    inline bool done() const noexcept {
        return index_ >= actions_.size();
    }

    inline void reset() noexcept {
        index_ = 0;
    }

private:
    // Action dispatch --------------------------------------------------------

    template <typename WS>
    static void execute_(WS& ws, const Connect& c) {
        ws.set_next_connect_result(c.result);
    }

    template <typename WS>
    static void execute_(WS& ws, const Message& m) {
        ws.emit_message(m.payload);
    }

    template <typename WS>
    static void execute_(WS& ws, const ErrorEvent& e) {
        ws.emit_error(e.error);
    }

    template <typename WS>
    static void execute_(WS& ws, const Close&) {
        ws.close();
    }

private:
    std::vector<Action> actions_;
    std::size_t index_{0};
};

} // namespace wirekrak::core::transport::test
