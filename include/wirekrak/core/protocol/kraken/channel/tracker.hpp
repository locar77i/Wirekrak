#pragma once

#include <unordered_map>
#include <unordered_set>
#include <cstdint>

#include "wirekrak/core/protocol/kraken/enums.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/symbol/intern.hpp"
#include "lcr/log/logger.hpp"

namespace wirekrak::core::protocol::kraken::channel {

class Manager {
public:

    // ============================================================
    // STATE MODEL
    // ============================================================

    enum class SymbolState : uint8_t {
        None = 0,
        PendingSubscribe,
        Active,
        PendingUnsubscribe
    };

    enum class EventType : uint8_t {
        OutboundSubscribe,
        OutboundUnsubscribe,
        SubscribeAck,
        UnsubscribeAck,
        Rejection
    };

    struct Event {
        EventType type;
        ctrl::req_id_t req_id;
        Symbol symbol;
        bool success = true;
    };

public:

    explicit Manager(Channel channel)
        : channel_(channel)
    {}

    // ============================================================
    // PUBLIC API → converts to events
    // ============================================================

    void outbound_subscribe(ctrl::req_id_t req_id, const RequestSymbols& symbols) {
        for (const auto& s : symbols) {
            on_event({EventType::OutboundSubscribe, req_id, s});
        }
    }

    void outbound_unsubscribe(ctrl::req_id_t req_id, const RequestSymbols& symbols) {
        for (const auto& s : symbols) {
            on_event({EventType::OutboundUnsubscribe, req_id, s});
        }
    }

    void process_subscribe_ack(ctrl::req_id_t req_id,
                               Symbol symbol,
                               bool success)
    {
        on_event({EventType::SubscribeAck, req_id, symbol, success});
    }

    void process_unsubscribe_ack(ctrl::req_id_t req_id,
                                 Symbol symbol,
                                 bool success)
    {
        on_event({EventType::UnsubscribeAck, req_id, symbol, success});
    }

    bool try_process_rejection(ctrl::req_id_t req_id,
                               Symbol symbol)
    {
        return on_event({EventType::Rejection, req_id, symbol});
    }

    // ============================================================
    // STATE QUERIES
    // ============================================================

    std::size_t total_symbols() const noexcept {
        std::size_t count = 0;
        for (const auto& [_, st] : states_) {
            if (st == SymbolState::Active ||
                st == SymbolState::PendingSubscribe)
                ++count;
        }
        return count;
    }

    std::size_t active_symbols() const noexcept {
        std::size_t count = 0;
        for (const auto& [_, st] : states_) {
            if (st == SymbolState::Active)
                ++count;
        }
        return count;
    }

    std::size_t pending_subscribe_symbols() const noexcept {
        std::size_t count = 0;
        for (const auto& [_, st] : states_) {
            if (st == SymbolState::PendingSubscribe)
                ++count;
        }
        return count;
    }

    std::size_t pending_unsubscribe_symbols() const noexcept {
        std::size_t count = 0;
        for (const auto& [_, st] : states_) {
            if (st == SymbolState::PendingUnsubscribe)
                ++count;
        }
        return count;
    }

    bool has_pending_requests() const noexcept {
        return pending_subscribe_symbols() ||
               pending_unsubscribe_symbols();
    }

    void clear_all() noexcept {
        states_.clear();
        req_map_.clear();
    }

private:

    // ============================================================
    // STATE MACHINE CORE
    // ============================================================

    bool on_event(const Event& e)
    {
        SymbolId sid = intern_symbol(e.symbol);
        auto& state = states_[sid];

        switch (e.type)
        {
            case EventType::OutboundSubscribe:
                if (state == SymbolState::None) {
                    state = SymbolState::PendingSubscribe;
                    req_map_[e.req_id] = sid;
                }
                return true;

            case EventType::OutboundUnsubscribe:
                if (state == SymbolState::PendingSubscribe) {
                    // cancel before ACK
                    state = SymbolState::None;
                }
                else if (state == SymbolState::Active) {
                    state = SymbolState::PendingUnsubscribe;
                    req_map_[e.req_id] = sid;
                }
                return true;

            case EventType::SubscribeAck:
                if (state == SymbolState::PendingSubscribe) {
                    state = e.success
                        ? SymbolState::Active
                        : SymbolState::None;
                }
                return true;

            case EventType::UnsubscribeAck:
                if (state == SymbolState::PendingUnsubscribe) {
                    state = e.success
                        ? SymbolState::None
                        : SymbolState::Active;
                }
                return true;

            case EventType::Rejection:
            {
                auto it = req_map_.find(e.req_id);
                if (it == req_map_.end())
                    return false;

                auto sid2 = it->second;
                auto& st = states_[sid2];

                if (st == SymbolState::PendingSubscribe)
                    st = SymbolState::None;
                else if (st == SymbolState::PendingUnsubscribe)
                    st = SymbolState::Active;

                req_map_.erase(it);
                return true;
            }
        }

        return false;
    }

private:
    Channel channel_;

    std::unordered_map<SymbolId, SymbolState> states_;
    std::unordered_map<ctrl::req_id_t, SymbolId> req_map_;
};

} // namespace
