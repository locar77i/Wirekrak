#pragma once

#include "wirekrak/core/types.hpp"

#include "wirekrak/protocol/kraken/trade/Subscribe.hpp"
#include "wirekrak/protocol/kraken/trade/Unsubscribe.hpp"
#include "wirekrak/protocol/kraken/trade/Response.hpp"
#include "wirekrak/protocol/kraken/trade/SubscribeAck.hpp"
#include "wirekrak/protocol/kraken/trade/UnsubscribeAck.hpp"

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
struct channel_of<protocol::kraken::trade::Subscribe> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<protocol::kraken::trade::Unsubscribe> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<protocol::kraken::trade::Response> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<protocol::kraken::trade::SubscribeAck> {
    static constexpr Channel value = Channel::Trade;
};

template<>
struct channel_of<protocol::kraken::trade::UnsubscribeAck> {
    static constexpr Channel value = Channel::Trade;
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
struct channel_traits<protocol::kraken::trade::Subscribe> {
    static constexpr Channel channel = Channel::Trade;
    using response_type = protocol::kraken::trade::Response;
};

template<>
struct channel_traits<protocol::kraken::trade::Unsubscribe> {
    static constexpr Channel channel = Channel::Trade;
    using response_type = protocol::kraken::trade::Response; // same dispatcher type
};

} // namespace kraken
} // namespace protocol
} // namespace wirekrak
