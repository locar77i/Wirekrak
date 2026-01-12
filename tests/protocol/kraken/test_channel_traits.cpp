#include <cassert>
#include <iostream>
#include <type_traits>

#include "wirekrak/protocol/kraken/channel_traits.hpp"

using namespace wirekrak::protocol::kraken;


namespace wirekrak {

/*
================================================================================
Kraken Channel Traits — Unit Tests
================================================================================

These tests validate compile-time channel mappings and traits for Kraken
WebSocket protocol message types.

Design goals enforced by this test suite:
  • Compile-time correctness — invalid mappings fail to compile
  • Zero runtime overhead — all checks use static_assert
  • Complete coverage — every public protocol message is validated
  • Dispatcher safety — request → response routing is deterministic
  • Negative coverage — non-request types must NOT have channel_traits

This guarantees protocol refactors cannot silently break dispatcher logic.
================================================================================
*/

// ============================================================================
// TRAIT DETECTION — DOES A TYPE DEFINE channel_traits?
// ============================================================================

template<typename T, typename = void>
struct has_channel_traits : std::false_type {};

template<typename T>
struct has_channel_traits<T, std::void_t<
    decltype(channel_traits<T>::channel),
    typename channel_traits<T>::response_type
>> : std::true_type {};

// ============================================================================
// CHANNEL OF<T> — MESSAGE → CHANNEL MAPPING
// ============================================================================

// ---- Trade ----
static_assert(channel_of_v<schema::trade::Subscribe>      == Channel::Trade);
static_assert(channel_of_v<schema::trade::Unsubscribe>    == Channel::Trade);
static_assert(channel_of_v<schema::trade::ResponseView>   == Channel::Trade);
static_assert(channel_of_v<schema::trade::SubscribeAck>   == Channel::Trade);
static_assert(channel_of_v<schema::trade::UnsubscribeAck> == Channel::Trade);

// ---- Book ----
static_assert(channel_of_v<schema::book::Subscribe>      == Channel::Book);
static_assert(channel_of_v<schema::book::Unsubscribe>    == Channel::Book);
static_assert(channel_of_v<schema::book::Response>       == Channel::Book);
static_assert(channel_of_v<schema::book::SubscribeAck>   == Channel::Book);
static_assert(channel_of_v<schema::book::UnsubscribeAck> == Channel::Book);

// ============================================================================
// CHANNEL NAME — STRING REPRESENTATION
// ============================================================================

// ---- Trade ----
static_assert(channel_name_of_v<schema::trade::Subscribe>    == "trade");
static_assert(channel_name_of_v<schema::trade::Unsubscribe>  == "trade");
static_assert(channel_name_of_v<schema::trade::ResponseView> == "trade");

// ---- Book ----
static_assert(channel_name_of_v<schema::book::Subscribe>   == "book");
static_assert(channel_name_of_v<schema::book::Unsubscribe> == "book");
static_assert(channel_name_of_v<schema::book::Response>    == "book");

// ============================================================================
// CHANNEL TRAITS — REQUEST → RESPONSE TYPE
// ============================================================================

// ---- Trade requests produce Response events ----
static_assert(channel_traits<schema::trade::Subscribe>::channel == Channel::Trade);
static_assert(std::is_same_v<
    channel_traits<schema::trade::Subscribe>::response_type,
    schema::trade::ResponseView
>);

static_assert(channel_traits<schema::trade::Unsubscribe>::channel == Channel::Trade);
static_assert(std::is_same_v<
    channel_traits<schema::trade::Unsubscribe>::response_type,
    schema::trade::ResponseView
>);

// ---- Book requests produce Response events ----
static_assert(channel_traits<schema::book::Subscribe>::channel == Channel::Book);
static_assert(std::is_same_v<
    channel_traits<schema::book::Subscribe>::response_type,
    schema::book::Response
>);

static_assert(channel_traits<schema::book::Unsubscribe>::channel == Channel::Book);
static_assert(std::is_same_v<
    channel_traits<schema::book::Unsubscribe>::response_type,
    schema::book::Response
>);

// ============================================================================
// NEGATIVE TRAITS — TYPES THAT MUST NOT HAVE channel_traits
// ============================================================================

// ---- Trade ----
static_assert(!has_channel_traits<schema::trade::Response>::value);
static_assert(!has_channel_traits<schema::trade::SubscribeAck>::value);
static_assert(!has_channel_traits<schema::trade::UnsubscribeAck>::value);

// ---- Book ----
static_assert(!has_channel_traits<schema::book::Response>::value);
static_assert(!has_channel_traits<schema::book::SubscribeAck>::value);
static_assert(!has_channel_traits<schema::book::UnsubscribeAck>::value);

} // namespace wirekrak

// ============================================================================
// RUNTIME HARNESS (CTest compatibility only)
// ============================================================================

int main() {
    std::cout << "[TEST] Channel traits compile-time validation passed.\n";
    std::cout << "[TEST] ALL CHANNEL TRAITS TESTS PASSED!\n";
    return 0;
}
