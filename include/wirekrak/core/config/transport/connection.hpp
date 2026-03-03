/*
================================================================================
Wirekrak Connection Configuration
================================================================================
*/
#pragma once

#include <cstddef>

#include "wirekrak/core/config/transport/websocket.hpp"


namespace wirekrak::core::transport {

// Capacity of the signal event ring buffer (number of signals)
inline constexpr static std::size_t SIGNAL_RING_CAPACITY = 64;

} // namespace wirekrak::core::transport
