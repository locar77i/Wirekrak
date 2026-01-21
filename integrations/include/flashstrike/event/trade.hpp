#pragma once

#include <cstdint>
#include <vector>
#include <atomic>
#include <cassert>

#include "flashstrike/types.hpp"


namespace flashstrike {
namespace event {

struct alignas(64) Trade {
    uint64_t seq_num{};           // 8 bytes
    OrderId  maker_order_id{};    // 4 bytes
    OrderId  taker_order_id{};    // 4 bytes
    UserId   maker_user_id{};     // 4 bytes
    UserId   taker_user_id{};     // 4 bytes
    Price    price{};             // 8 bytes
    Quantity qty{};               // 8 bytes
    uint64_t ts_engine_ns{};      // 8 bytes
    Fee      maker_fee{};         // 4 bytes
    Fee      taker_fee{};         // 4 bytes
    Side     taker_side{};        // 1 byte
    // padding
    uint8_t  pad_[7]{};           // 7 bytes to align to 64 bytes total

    // Constructors
    constexpr Trade() noexcept = default;

    constexpr Trade(uint64_t seq, OrderId maker, OrderId taker, UserId maker_uid, UserId taker_uid, Price p, Quantity q, uint64_t ts, Fee mfee, Fee tfee, Side tside) noexcept
        : seq_num(seq), maker_order_id(maker), taker_order_id(taker),
          maker_user_id(maker_uid), taker_user_id(taker_uid),
          price(p), qty(q), ts_engine_ns(ts),
          maker_fee(mfee), taker_fee(tfee), taker_side(tside)
    {}

    // Helpers
    inline void debug_dump(const Trade& e, std::ostream& os) {
        os << "[Trade] seq=" << e.seq_num
          << " price=" << e.price
          << " qty=" << e.qty
          << " taker_side=" << (e.taker_side == Side::BID ? "BID" : "ASK")
          << " maker_id=" << e.maker_order_id
          << " taker_id=" << e.taker_order_id
          << " ts=" << e.ts_engine_ns
          << " maker_fee=" << e.maker_fee
          << " taker_fee=" << e.taker_fee
          << "\n";
    }
};
static_assert(sizeof(Trade) == 64, "Trade must be 64 bytes");
static_assert(alignof(Trade) == 64, "Trade must be cache-line aligned");
static_assert(std::is_trivially_copyable_v<Trade>, "Trade must be trivially copyable");
static_assert(std::is_standard_layout_v<Trade>, "Trade must be standard layout");

} // namespace event
} // namespace flashstrike
