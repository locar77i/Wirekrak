#pragma once

#include <cstdint>
#include <cmath>
#include <ostream>
#include <assert.h>


namespace flashstrike {

// Enumerations for order attributes
enum class Side : uint8_t {
    BID, // Buy side
    ASK  // Sell side
};
inline const char* to_string(Side side) {
    return (side == Side::BID) ? "BID" : "ASK";
}

enum class RequestType : uint8_t {
    NEW_ORDER = 0,
    MODIFY_ORDER_PRICE = 1,
    MODIFY_ORDER_QUANTITY = 2,
    CANCEL_ORDER = 3
};

enum class OrderType : uint8_t {
    LIMIT, // Limit order
    MARKET // Market order
};
inline const char* to_string(OrderType type) {
    return (type == OrderType::LIMIT) ? "LIMIT" : "MARKET";
}

enum class TimeInForce : uint8_t {
    GTC, // Good Till Cancelled
    IOC, // Immediate Or Cancel
    FOK  // Fill Or Kill
};
inline const char* to_string(TimeInForce tif) {
    switch(tif) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
        default: return "UNKNOWN_TIF";
    }
}

enum class OperationStatus : uint8_t {
    SUCCESS,          // Operation completed fully
    FULL_FILL,        // Order fully filled (for inserts)
    PARTIAL_FILL,     // Order partially filled (for inserts)
    NO_MATCH,         // Market/limit order could not match
    NOT_FOUND,        // Cancel/modify: order not found
    UNCHANGED,        // No change made (e.g., modify to same price/qty)
    INVALID_ORDER,    // Invalid input (e.g., negative qty, invalid side)
    BOOK_FULL,        // Cannot insert: order book is full
    BAD_ALLOC,        // Cannot allocate from order pool
    IDMAP_FULL,       // OrderIdMap is full
    REJECTED          // General rejection (optional catch-all)
};
inline const char* to_string(OperationStatus status) {
    switch(status) {
        case OperationStatus::SUCCESS: return "SUCCESS";
        case OperationStatus::FULL_FILL: return "FULL_FILL";
        case OperationStatus::PARTIAL_FILL: return "PARTIAL_FILL";
        case OperationStatus::NO_MATCH: return "NO_MATCH";
        case OperationStatus::NOT_FOUND: return "NOT_FOUND";
        case OperationStatus::UNCHANGED: return "UNCHANGED";
        case OperationStatus::INVALID_ORDER: return "INVALID_ORDER";
        case OperationStatus::BOOK_FULL: return "BOOK_FULL";
        case OperationStatus::BAD_ALLOC: return "BAD_ALLOC";
        case OperationStatus::IDMAP_FULL: return "IDMAP_FULL";
        case OperationStatus::REJECTED: return "REJECTED";
        default: return "UNKNOWN_STATUS";
    }
}

// Type definitions for clarity and consistency
using EventId = std::uint64_t;   // Unique event identifier

using OrderId = std::uint32_t;   // Unique order identifier
using OrderIdx = std::int32_t;   // Index in the order pool

using Price = std::int64_t;      // Scaled integer price ticks
using Quantity = std::int64_t;   // Scaled integer qty ticks
using Notional = std::int64_t;   // Scaled integer used for the product (Price * Quantity) - check for overflow in debug builds

using Trades = std::uint32_t;    // Number of trades executed
using Timestamp = uint64_t;
using UserId = std::uint32_t;    // Unique user identifier
using Fee = std::int32_t;        // Fee in ticks (can be negative for rebates)


} // namespace flashstrike
