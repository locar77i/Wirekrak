#pragma once

#include "flashstrike/events.hpp"
#include "flashstrike/matching_engine/conf/partition_plan.hpp"
#include "flashstrike/matching_engine/order_book.hpp"
#include "flashstrike/matching_engine/telemetry/init.hpp"
#include "flashstrike/matching_engine/telemetry/manager.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/memory/footprint.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/sequence.hpp"
#include "lcr/log/Logger.hpp"

using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {

size_t constexpr TRADES_RING_BUFFER_SIZE = 1 << 10;   // 1024 events (must be power of two)


// The Matching Engine interface
class Manager {
public:
    // Constructor
    Manager(std::uint64_t max_orders, const conf::Instrument& instrument, std::uint32_t target_num_partitions, matching_engine::Telemetry& metrics)
        : start_ns_(monotonic_clock::instance().now_ns())
        , pplan_()
        , instrument_(instrument)
        , normalized_instrument_(pplan_.compute(instrument, target_num_partitions))
        , book_(max_orders, pplan_.num_partitions(), pplan_.partition_size(), pplan_.partition_bits(), metrics)
        , seq_gen_(1) // start sequence numbers at 1
        , init_metrics_updater_(metrics.init_metrics)
        , manager_metrics_updater_(metrics.manager_metrics)
    {
        WK_TRACE("Manager initialized for '" << instrument_.name << "' with parameters: "
            << "  price_max_units=" << instrument_.price_max_units
            << ", price_tick_units=" << instrument_.price_tick_units
            << ", partition_bits=" << pplan_.partition_bits()
            << ", num_partitions=" << pplan_.num_partitions()
            << ", partition_size=" << pplan_.partition_size()
            << ", num_ticks=" << pplan_.num_ticks()
            << ", max_orders=" << max_orders
            << ", trades_ring_capacity=" << trades_ring_.capacity()
        );
        //pplan_.debug_dump(std::cout);
        //instrument_.debug_dump(std::cout);
        //normalized_instrument_.debug_dump(std::cout);
        init_metrics_updater_.on_create_matching_engine(start_ns_, memory_usage().total_bytes());
        init_metrics_updater_.on_create_trades_ring(trades_ring_.capacity(), trades_ring_.memory_usage().total_bytes());
        //init_metrics_updater_.dump(instrument_.get_symbol(), std::cout);
    }

    // OrderBook accessor
    OrderBook& book() { return book_; }

    [[nodiscard]] inline OperationStatus process_order(OrderId order_id, OrderType type, Side side, Price price, Quantity qty, OrderIdx &order_idx_out) {
        Order order{};
        order.id      = order_id;
        order.type    = type;
        order.side    = side;
        order.price   = price;
        order.qty     = qty;
        order.filled  = 0;
        Trades trade_count;
        Price last_price;
        OperationStatus status;
        if (order.side == Side::BID) {
            status = process_order<Side::BID>(order, trade_count, last_price, order_idx_out);
        } else {
            status = process_order<Side::ASK>(order, trade_count, last_price, order_idx_out);
        }
        return status;
    }

    template<Side SIDE>
    [[nodiscard]] inline OperationStatus process_order(Order &order, Trades &trades_out, Price &last_price_out, OrderIdx &order_idx_out) noexcept {
#ifdef ENABLE_FS_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        OperationStatus status;
        if (!validate_order_(order)) [[unlikely]] {
            WK_TRACE("Rejecting order id=" << order.id << " due to validation failure: "
                      << " type=" << to_string(order.type)
                      << ", side=" << to_string(order.side)
                      << ", price=" << order.price
                      << ", qty=" << order.qty << "\n");
            status = OperationStatus::REJECTED;
            order_idx_out = INVALID_INDEX;
#ifdef ENABLE_FS_METRICS
            manager_metrics_updater_.on_process_on_fly_order(start_ns, status);
#endif
            return status;
        }
        // Dispatch templated match based on incoming order side
        status = match_order_<SIDE>(order, trades_out, last_price_out); // internally uses OPP_SIDE = ASK
        // Market orders or fully filled orders do not rest in the book
        if(order.type == OrderType::MARKET || status == OperationStatus::FULL_FILL) {
            order_idx_out = INVALID_INDEX;
            // Launch noinsert/trade event here
#ifdef ENABLE_FS_METRICS
            manager_metrics_updater_.on_process_on_fly_order(start_ns, status);
#endif
            return status;
        }
        // Remaining limit quantity → allocate in order pool and insert into book
        auto insert_status = book_.insert_order<SIDE>(order.id, order.price, order.qty, order.filled, order_idx_out);
        if (insert_status == OperationStatus::SUCCESS) {
            WK_TRACE("Insert:Done!  Price: " << order.price << ", filled: " << order.filled << ", remaining: " << order.qty << ", order idx: " << order_idx_out << ", trades: " << trades_out << ", last price: " << last_price_out);
            if (order.filled > 0) {
                status = (order.qty > 0) ? OperationStatus::PARTIAL_FILL : OperationStatus::FULL_FILL;
            }
        }
        else {
            WK_TRACE("Insert:Failed! status=" << to_string(insert_status) << ", order id: " << order.id);
            status = insert_status; // e.g., BAD_ALLOC or IDMAP_FULL
        }
#ifdef ENABLE_FS_METRICS
        manager_metrics_updater_.on_process_resting_order(start_ns, status);
#endif
        return status;
    }

    [[nodiscard]] inline OperationStatus modify_order_price(OrderId orderid, Price new_price) noexcept {
#ifdef ENABLE_FS_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        assert(new_price > 0 && "new_price must be > 0 when modifying");
        assert(new_price < normalized_instrument_.price_max_scaled() && "new_price must be < price_max_scaled");
        // First, let the book perform the actual reindexing
        Order* o = nullptr;
        OperationStatus status = book_.reprice_order(orderid, new_price, &o);
        if (status != OperationStatus::SUCCESS) {
#ifdef ENABLE_FS_METRICS
        manager_metrics_updater_.on_modify_order_price(start_ns, status);
#endif
            return status; // e.g., NOT_FOUND or REJECTED
        }
        // Check for crossing opportunity
        Trades trades;
        Price last_price;
        OperationStatus match_status;
        if (o->side == Side::BID) {
            match_status = match_resting_order_<Side::BID>(*o, trades, last_price);
        } else {
            match_status = match_resting_order_<Side::ASK>(*o, trades, last_price);
        }
        if (match_status == OperationStatus::PARTIAL_FILL || match_status == OperationStatus::FULL_FILL) {
            status = match_status;
        }
#ifdef ENABLE_FS_METRICS
        manager_metrics_updater_.on_modify_order_price(start_ns, status);
#endif
        return status;
}

    [[nodiscard]] inline OperationStatus modify_order_quantity(OrderId orderid, Quantity new_qty) noexcept {
#ifdef ENABLE_FS_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        assert(new_qty > 0 && "new_qty must be > 0 when modifying");
        OperationStatus status = book_.resize_order(orderid, new_qty);
#ifdef ENABLE_FS_METRICS
        manager_metrics_updater_.on_modify_order_quantity(start_ns, status);
#endif
        return status;
    }

    [[nodiscard]] inline OperationStatus cancel_order(OrderId orderid) noexcept {
#ifdef ENABLE_FS_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        OperationStatus status = book_.remove_order(orderid);
#ifdef ENABLE_FS_METRICS
        manager_metrics_updater_.on_cancel_order(start_ns, status);
#endif
        return status;
    }

    inline void on_periodic_maintenance() noexcept {
        manager_metrics_updater_.on_every_n_requests(book_.order_pool().used(), book_.order_id_map().used(), book_.partition_pool().used(), trades_ring_.used());
    }

    // Accessors
    inline const conf::PartitionPlan& partition_plan() const noexcept  { return pplan_; }
    inline const conf::Instrument& instrument() const noexcept { return instrument_; }
    inline const conf::NormalizedInstrument& normalized_instrument() const noexcept { return normalized_instrument_; }

    // normalize price/quantity helpers
    inline Price normalize_price(double user_price_units) const noexcept {
        return instrument().normalize_price(user_price_units);
    }

    inline Quantity normalize_quantity(double user_qty_units) const noexcept {
        return instrument().normalize_quantity(user_qty_units);
    }

    // Access to trade events (ring buffer)
    inline lcr::lockfree::spsc_ring<TradeEvent, TRADES_RING_BUFFER_SIZE>& trades_ring() noexcept { return trades_ring_; }
    inline const lcr::lockfree::spsc_ring<TradeEvent, TRADES_RING_BUFFER_SIZE>& trades_ring() const noexcept { return trades_ring_; }

    inline lcr::memory::footprint memory_usage() const noexcept {
        lcr::memory::footprint mf{
            .static_bytes = sizeof(Manager),
            .dynamic_bytes = 0
        };
        // Add memory usage of each sub-component
        mf.add_dynamic(book_);
        mf.add_dynamic(trades_ring_);
        return mf;
    }

private:
    Timestamp start_ns_;
    conf::PartitionPlan pplan_;
    conf::Instrument instrument_;
    conf::NormalizedInstrument normalized_instrument_;
    OrderBook book_;
    lcr::lockfree::spsc_ring<TradeEvent, TRADES_RING_BUFFER_SIZE> trades_ring_{};
    lcr::sequence seq_gen_;

    telemetry::InitUpdater init_metrics_updater_;
    telemetry::ManagerUpdater manager_metrics_updater_;

    // Private helpers ----------------------------
    [[nodiscard]] inline bool validate_order_(const Order& o) const noexcept {
        if (o.price < normalized_instrument_.price_min_scaled() || o.price > normalized_instrument_.price_max_scaled()) {
            WK_DEBUG("Rejecting order id " << o.id << ": price " << o.price << " out of bounds [" << normalized_instrument_.price_min_scaled() << ", " << normalized_instrument_.price_max_scaled() << "]");
            return false;
        }
/*
        if (o.price % normalized_instrument_.price_tick_size() != 0) {
            WK_DEBUG("Rejecting order id " << o.id << ": price " << o.price << " not aligned with tick size " << normalized_instrument_.price_tick_size());
            return false;
        }
*/
        if (o.qty < normalized_instrument_.qty_min_scaled() || o.qty > normalized_instrument_.qty_max_scaled()) {
            WK_DEBUG("Rejecting order id " << o.id << ": qty " << o.qty << " out of bounds [" << normalized_instrument_.qty_min_scaled() << ", " << normalized_instrument_.qty_max_scaled() << "]");
            return false;
        }
/*
        if (o.qty % normalized_instrument_.qty_tick_size() != 0) {
            WK_DEBUG("Rejecting order id " << o.id << ": qty " << o.qty << " not aligned with tick size " << normalized_instrument_.qty_tick_size());
            return false;
        }
*/
        if ((o.price * o.qty) < normalized_instrument_.min_notional()) {
            WK_DEBUG("Rejecting order id " << o.id << ": notional " << (o.price * o.qty) << " below min notional " << normalized_instrument_.min_notional());
            return false;
        }
        return true;
    }

    // Templated matching logic based on incoming order side
    template<Side SIDE>
    inline OperationStatus match_order_(Order& incoming, Trades &trades_out, Price &last_price_out) {
#ifdef ENABLE_FS2_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        trades_out = 0;
        constexpr Side OPP_SIDE = (SIDE == Side::BID) ? Side::ASK : Side::BID;
        PriceLevel* best = book_.get_store<OPP_SIDE>().get_best_price_level();
        // Early exit if no crossing (LIMIT orders only)
        if (incoming.type == OrderType::LIMIT && best && !PriceComparator<SIDE>::crosses(incoming.price, best->get_price())) {
            last_price_out = 0;
#ifdef ENABLE_FS2_METRICS
            manager_metrics_updater_.on_match_order(start_ns, trades_out, OperationStatus::NO_MATCH);
#endif
            return OperationStatus::NO_MATCH;
        }
        while (best && best->get_head_idx() != INVALID_INDEX && incoming.qty > 0) {
            WK_TRACE("Matching against price level: " << best->get_price() << " with total qty: " << best->total_quantity() << " (incoming qty: " << incoming.qty << ")");
            Order& resting = book_.get_order(best->get_head_idx());
            Quantity trade_qty = std::min(incoming.qty, resting.qty);
            // Update quantities
            incoming.qty -= trade_qty;
            incoming.filled += trade_qty;
            resting.qty -= trade_qty;
            resting.filled += trade_qty;
            // update total quantity at price level
            best->subtract_quantity(trade_qty);
            // Update match stats
            last_price_out = best->get_price();
            // Check if resting order fully filled
            if (resting.qty == 0) {
                if (book_.remove_order(resting.id) != OperationStatus::SUCCESS) {
                    WK_TRACE("Match incoming order: Error removing fully filled resting order id: " << resting.id);
                }
#ifdef ENABLE_FS2_METRICS
                manager_metrics_updater_.on_remove_order_after_match();
#endif
            }
            ++trades_out;
            emit_trade_event_(resting.id, incoming.id, last_price_out, trade_qty, incoming.side);
            // Move to next best price level if current exhausted
            best = book_.get_store<OPP_SIDE>().get_best_price_level();
        }
        OperationStatus status = OperationStatus::NO_MATCH;
        if (incoming.filled > 0) {
            status = (incoming.qty == 0) ? OperationStatus::FULL_FILL : OperationStatus::PARTIAL_FILL;
        }
#ifdef ENABLE_FS2_METRICS
        manager_metrics_updater_.on_match_order(start_ns, trades_out, status);
#endif
        return status;
    }

    template<Side SIDE>
    inline OperationStatus match_resting_order_(Order& resting, Trades &trades_out, Price &last_price_out) noexcept {
        Quantity qty = resting.qty;
        OperationStatus status = match_order_<SIDE>(resting, trades_out, last_price_out);
        // Update the price level total quantity if There are kind of matches
        if (status == OperationStatus::PARTIAL_FILL || status == OperationStatus::FULL_FILL) {
            book_.get_store<SIDE>().get_level(resting.price).subtract_quantity(qty - resting.qty);
        }
        // Remove resting order from book if fully filled
        if (status == OperationStatus::FULL_FILL) {
            if (book_.remove_order(resting.id) != OperationStatus::SUCCESS) {
                WK_TRACE("Match resting order: Error removing fully filled resting order id: " << resting.id);
            }
#ifdef ENABLE_FS2_METRICS
            manager_metrics_updater_.on_remove_order_after_match();
#endif
        }

        return status;
    }

    inline void emit_trade_event_(OrderId maker_order_id, OrderId taker_order_id, Price price, Quantity qty, Side taker_side) noexcept {
        // Generate trade event. Next free slot (blocking if needed)
        // Hot path never has watchdog logic. In the future, we can add:
        // - Producer spin-loops in the blocking call if buffer full.
        // - A separate monitoring thread (watchdog thread) periodically inspects queue depth / lag.
        // - If depth > threshold, it raises alerts, dumps state, or kills the process.
        // This keeps the producer hot path as clean as possible. All safety / monitoring goes in the cold path.
        TradeEvent ev{};
        ev.seq_num = seq_gen_.next();
        ev.maker_order_id = maker_order_id;
        ev.taker_order_id = taker_order_id;
        ev.price = price;
        ev.qty = qty;
        ev.taker_side = taker_side;
        size_t spins = 0;
        while (!trades_ring_.push(ev)) {
            ++spins;
            if(spins > SPINS_GUESS) {
                std::this_thread::yield();
                spins = 0;
            }
            else {
                lcr::system::cpu_relax();
            }
        }
    }
};

} // namespace matching_engine
} // namespace flashstrike
