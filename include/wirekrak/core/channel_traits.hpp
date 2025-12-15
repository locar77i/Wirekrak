#pragma once

#include "wirekrak/core/types.hpp"

#include "wirekrak/schema/trade/Subscribe.hpp"
#include "wirekrak/schema/trade/Unsubscribe.hpp"
#include "wirekrak/schema/trade/Response.hpp"
#include "wirekrak/schema/trade/SubscribeAck.hpp"
#include "wirekrak/schema/trade/UnsubscribeAck.hpp"

namespace wirekrak {

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
struct channel_of<schema::trade::Response> {
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
    using response_type = schema::trade::Response;
};

template<>
struct channel_traits<schema::trade::Unsubscribe> {
    static constexpr Channel channel = Channel::Trade;
    using response_type = schema::trade::Response; // same dispatcher type
};

} // namespace wirekrak
