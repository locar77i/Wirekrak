#pragma once

#include "wirekrak/protocol/kraken/enums/channel.hpp"

#include "wirekrak/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/protocol/kraken/schema/trade/unsubscribe.hpp"
#include "wirekrak/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/protocol/kraken/schema/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/schema/trade/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/schema/book/subscribe.hpp"
#include "wirekrak/protocol/kraken/schema/book/unsubscribe.hpp"
#include "wirekrak/protocol/kraken/schema/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/schema/book/update.hpp"
#include "wirekrak/protocol/kraken/schema/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/schema/book/unsubscribe_ack.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {

// ============================================================================
// CHANNEL OF (MESSAGE → CHANNEL MAPPING)
// ============================================================================

// Primary template (undefined on purpose)
template<typename T>
struct channel_of;

// Convenient alias
template<typename T>
inline constexpr Channel channel_of_v = channel_of<T>::value;

// ---------------------------------------------------------------------------
// TRADE channel mappings
// ---------------------------------------------------------------------------

template<>
struct channel_of<trade::Subscribe> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<trade::Unsubscribe> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<trade::Response> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<trade::Trade> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<trade::SubscribeAck> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<trade::UnsubscribeAck> {
    static constexpr Channel value = Channel::Trade;
};


// ---------------------------------------------------------------------------
// BOOK channel mappings
// ---------------------------------------------------------------------------

template<>
struct channel_of<book::Subscribe> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<book::Unsubscribe> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<book::Snapshot> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<book::Update> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<book::SubscribeAck> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<book::UnsubscribeAck> {
    static constexpr Channel value = Channel::Book;
};


// String representation
template<typename T>
inline constexpr std::string_view channel_name_of_v = to_string(channel_of_v<T>);


// ============================================================================
// CHANNEL TRAITS (SUBSCRIBE REQUEST → RESPONSE TYPE)
// ============================================================================

// Primary template
template<typename RequestT>
struct channel_traits;

// ---------------------------------------------------------------------------
// TRADE: Subscribe → Trade
// ---------------------------------------------------------------------------

template<>
struct channel_traits<trade::Subscribe> {
    static constexpr Channel channel = Channel::Trade;
    using response_type = trade::Trade;
};

template<>
struct channel_traits<trade::Unsubscribe> {
    static constexpr Channel channel = Channel::Trade;
    using response_type = trade::Trade;
};


// ---------------------------------------------------------------------------
// BOOK: Subscribe → Update
// ---------------------------------------------------------------------------

template<>
struct channel_traits<book::Subscribe> {
    static constexpr Channel channel = Channel::Book;
    using response_type = book::Update;
};

template<>
struct channel_traits<book::Unsubscribe> {
    static constexpr Channel channel = Channel::Book;
    using response_type = book::Update;
};

} // namespace kraken
} // namespace protocol
} // namespace wirekrak
