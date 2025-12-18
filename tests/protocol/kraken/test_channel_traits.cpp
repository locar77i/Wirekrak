#include <cassert>
#include <iostream>
#include <type_traits>

#include "wirekrak/protocol/kraken/channel_traits.hpp"

#include "wirekrak/protocol/kraken/trade/subscribe.hpp"
#include "wirekrak/protocol/kraken/trade/unsubscribe.hpp"
#include "wirekrak/protocol/kraken/trade/response.hpp"
#include "wirekrak/protocol/kraken/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/trade/unsubscribe_ack.hpp"

#include "wirekrak/protocol/kraken/book/subscribe.hpp"
#include "wirekrak/protocol/kraken/book/unsubscribe.hpp"
#include "wirekrak/protocol/kraken/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/book/update.hpp"
#include "wirekrak/protocol/kraken/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/book/unsubscribe_ack.hpp"

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

This test guarantees that protocol refactors cannot silently break channel
dispatch logic, a critical invariant for high-performance market data systems.
================================================================================
*/

// ============================================================================
// CHANNEL OF<T> — MESSAGE → CHANNEL MAPPING
// ============================================================================

// ---- Trade ----
static_assert(channel_of_v<trade::Subscribe>        == Channel::Trade);
static_assert(channel_of_v<trade::Unsubscribe>      == Channel::Trade);
static_assert(channel_of_v<trade::Response>         == Channel::Trade);
static_assert(channel_of_v<trade::SubscribeAck>     == Channel::Trade);
static_assert(channel_of_v<trade::UnsubscribeAck>   == Channel::Trade);

// ---- Book ----
static_assert(channel_of_v<book::Subscribe>         == Channel::Book);
static_assert(channel_of_v<book::Unsubscribe>       == Channel::Book);
static_assert(channel_of_v<book::Snapshot>          == Channel::Book);
static_assert(channel_of_v<book::Update>            == Channel::Book);
static_assert(channel_of_v<book::SubscribeAck>      == Channel::Book);
static_assert(channel_of_v<book::UnsubscribeAck>    == Channel::Book);

// ============================================================================
// CHANNEL NAME — STRING REPRESENTATION
// ============================================================================

static_assert(channel_name_of_v<trade::Subscribe> == "trade");
static_assert(channel_name_of_v<book::Subscribe>  == "book");

// ============================================================================
// CHANNEL TRAITS — REQUEST → RESPONSE TYPE
// ============================================================================

// ---- Trade ----
static_assert(channel_traits<trade::Subscribe>::channel == Channel::Trade);
static_assert(std::is_same_v<
    channel_traits<trade::Subscribe>::response_type,
    trade::Response
>);

static_assert(channel_traits<trade::Unsubscribe>::channel == Channel::Trade);
static_assert(std::is_same_v<
    channel_traits<trade::Unsubscribe>::response_type,
    trade::Response
>);

// ---- Book ----
static_assert(channel_traits<book::Subscribe>::channel == Channel::Book);
static_assert(std::is_same_v<
    channel_traits<book::Subscribe>::response_type,
    book::Update
>);

static_assert(channel_traits<book::Unsubscribe>::channel == Channel::Book);
static_assert(std::is_same_v<
    channel_traits<book::Unsubscribe>::response_type,
    book::Update
>);

} // namespace wirekrak


// ============================================================================
// RUNTIME HARNESS (CTest compatibility only)
// ============================================================================

int main() {
    std::cout << "[TEST] Channel traits compile-time validation passed.\n";
    std::cout << "[TEST] ALL CHANNEL TRAITS TESTS PASSED!\n";
    return 0;
}
