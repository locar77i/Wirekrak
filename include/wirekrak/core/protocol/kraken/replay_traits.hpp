#pragma once

#include "wirekrak/core/protocol/replay_traits.hpp"

#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe.hpp"

#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe.hpp"

namespace wirekrak::core::protocol {

// ============================================================================
// REPLAY TRAITS (REQUEST → PERSISTENT INTENT MAPPING)
// ============================================================================
//
// Maps a protocol request type to the corresponding subscription type
// stored in the replay database.
//
// This defines which requests contribute to persistent protocol state.
//
// Key semantics:
// • Subscribe      → stored as-is
// • Unsubscribe    → maps to corresponding Subscribe type
//
// This trait is protocol-specific and must be specialized per exchange.
//
// ============================================================================


// ---------------------------------------------------------------------------
// TRADE channel
// ---------------------------------------------------------------------------

template<>
struct replay_traits<kraken::schema::trade::Subscribe> {
    using type = kraken::schema::trade::Subscribe;
};

template<>
struct replay_traits<kraken::schema::trade::Unsubscribe> {
    using type = kraken::schema::trade::Subscribe;
};


// ---------------------------------------------------------------------------
// BOOK channel
// ---------------------------------------------------------------------------

template<>
struct replay_traits<kraken::schema::book::Subscribe> {
    using type = kraken::schema::book::Subscribe;
};

template<>
struct replay_traits<kraken::schema::book::Unsubscribe> {
    using type = kraken::schema::book::Subscribe;
};

} // namespace wirekrak::core::protocol
