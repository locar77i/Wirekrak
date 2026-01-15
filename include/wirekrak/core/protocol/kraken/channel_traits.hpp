#pragma once

#include "wirekrak/core/protocol/kraken/enums/channel.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response_view.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/response.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe_ack.hpp"


namespace wirekrak::core {
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
struct channel_of<schema::trade::Subscribe> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<schema::trade::Unsubscribe> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<schema::trade::ResponseView> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<schema::trade::SubscribeAck> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<schema::trade::UnsubscribeAck> {
    static constexpr Channel value = Channel::Trade;
};


// ---------------------------------------------------------------------------
// BOOK channel mappings
// ---------------------------------------------------------------------------

template<>
struct channel_of<schema::book::Subscribe> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<schema::book::Unsubscribe> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<schema::book::Response> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<schema::book::SubscribeAck> {
    static constexpr Channel value = Channel::Book;
};

template<>
struct channel_of<schema::book::UnsubscribeAck> {
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
// TRADE: Subscribe → Response
// ---------------------------------------------------------------------------

template<>
struct channel_traits<schema::trade::Subscribe> {
    static constexpr Channel channel = Channel::Trade;
    using response_type = schema::trade::ResponseView;
};

template<>
struct channel_traits<schema::trade::Unsubscribe> {
    static constexpr Channel channel = Channel::Trade;
    using response_type = schema::trade::ResponseView;
};


// ---------------------------------------------------------------------------
// BOOK: Subscribe → Response
// ---------------------------------------------------------------------------

template<>
struct channel_traits<schema::book::Subscribe> {
    static constexpr Channel channel = Channel::Book;
    using response_type = schema::book::Response;
};

template<>
struct channel_traits<schema::book::Unsubscribe> {
    static constexpr Channel channel = Channel::Book;
    using response_type = schema::book::Response;
};

} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core
