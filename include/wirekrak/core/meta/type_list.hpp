#pragma once

namespace wirekrak::core::meta {

// ------------------------------------------------------------
// type_list
// ------------------------------------------------------------
//
// Minimal compile-time type container.
//
// Design goals:
// • Zero runtime footprint
// • No behavior beyond structural composition
// • Serves as a foundation for higher-level meta utilities
//
// ------------------------------------------------------------

template<class... Ts>
struct type_list {};

} // namespace wirekrak::core::meta
