#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iostream>

#include "wirekrak.hpp"
using namespace wirekrak::lite;

#include "common/cli/book.hpp"

#include "flashstrike/globals.hpp"
#include "flashstrike/matching_engine/conf/partition_plan.hpp"
#include "flashstrike/matching_engine/order_book.hpp"
#include "lcr/sequence.hpp"

namespace fs = flashstrike;
namespace fme = flashstrike::matching_engine;

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}


// --------------------------------------------------------------------------------
// Gateway class: handles order book updates and feeds them to the matching engine
// --------------------------------------------------------------------------------

namespace flashstrike {

class Gateway {
    // uint64_t max_orders constant defined:
    static constexpr uint64_t max_orders = 1ull << 19;       // 2^19 = 524,288 orders. Ideally must be factor of 2 for best performance.
    static constexpr uint32_t target_num_partitions = 256;   // number of partitions

public:
    Gateway(const std::string& instrument_name)
        : pplan_()
        , instrument_(fs::get_instrument_by_name(instrument_name))
        , normalized_instrument_(pplan_.compute(instrument_, target_num_partitions))
        , order_book_(max_orders, pplan_.num_partitions(), pplan_.partition_size(), pplan_.partition_bits(), metrics_, g_metrics_)
    {
    }

    void on_book_level(const BookLevel& lvl) {
        fs::OrderIdx order_idx_out;

        if (lvl.quantity == 0.0) {
            ++omitted_;
            return;
        }

        if (lvl.book_side == wirekrak::lite::Side::Buy) {
            insert_<fs::Side::BID>(lvl, order_idx_out);
        } else {
            insert_<fs::Side::ASK>(lvl, order_idx_out);
        }
    }

    // Accessors
    fme::telemetry::OrderBook& metrics() {
        return metrics_;
    }

private:
    fme::conf::PartitionPlan pplan_;
    fme::conf::Instrument instrument_;
    fme::conf::NormalizedInstrument normalized_instrument_;

    fme::Telemetry g_metrics_;
    fme::telemetry::OrderBook metrics_;
    fme::OrderBook order_book_;

    // demo / metrics
    std::size_t inserted_ = 0;
    std::size_t omitted_ = 0;

private:
    template<fs::Side SIDE>
    inline void insert_(const BookLevel& lvl, fs::OrderIdx& order_idx_out) {
        fme::Order order{};
        generate_order_<SIDE>(order,
            instrument_.normalize_price(lvl.price),
            instrument_.normalize_quantity(lvl.quantity)
        );

        (void)order_book_.insert_order<SIDE>(order.id, order.price, order.qty, order.filled, order_idx_out);
        ++inserted_;
        if (inserted_ && inserted_ % 1000 == 0) {
            WK_INFO("[WWE] Inserted " << inserted_ << " orders... (" << omitted_ << " orders omitted)");
        }
        if (inserted_ && inserted_ % 10000 == 0) {
                metrics_.dump("Order Book", std::cout);
        }
    }

    template<fs::Side SIDE>
    inline void generate_order_(fme::Order &out, fs::Price price, fs::Quantity qty) {
        // Sequential order ID generator
        static lcr::sequence id_seq_{};
        // Fill order fields
        out.id      = id_seq_.next();
        out.type    = fs::OrderType::LIMIT;
        out.side    = SIDE;
        out.price   = price;
        out.qty     = qty;
        out.filled  = 0;
    }

};

} // namespace flashstrike


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


int main(int argc, char** argv)
{
    WK_WARN("===  Wirekrak Kraken Book + Flashstrike Order Book Example ===");

    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------
    // CLI parsing
    // -------------------------------------------------------------
    const auto& params = wirekrak::cli::book::configure(argc, argv,
        "This example show you how to integrate Flashstrike Order Book with Wirekrak Kraken WebSocket API v2.\n",
        "Subscribes to order book updates for a given symbol and feeds them to Flashstrike Order Book.\n"
    );
    params.dump("=== Wirekrak & Flashstrike Parameters ===", std::cout);
    const std::string& symbol = params.symbols.back(); // use last symbol for simplicity

    // -------------------------------------------------------------
    // Gateway setup
    // -------------------------------------------------------------
    WK_DEBUG("[ME] Initializing flashstrike::Gateway...");
    flashstrike::Gateway gateway(symbol);

    // -------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------
    WK_DEBUG("[ME] Initializing Client...");
    Client client{params.url};

    client.on_error([](const Error& err) {
        WK_WARN("[wirekrak-lite] error: " << err.message);
    });

    if (!client.connect()) {
        WK_ERROR("[wirekrak-lite] Failed to connect");
        return -1;
    }

    // -------------------------------------------------------------
    // Subscribe to book updates
    // -------------------------------------------------------------
    auto book_handler = [&](const BookLevel& lvl) {
        gateway.on_book_level(lvl);
    };

    client.subscribe_book({symbol}, book_handler, params.snapshot);

    // -------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------
    while (running.load()) {
        client.poll();                // 1) Poll Wirekrak client (required to process incoming messages)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 2) Sleep a bit to avoid busy loop
    }

    // -------------------------------------------------------------
    // Unsubscribe from book updates
    // -------------------------------------------------------------
    client.unsubscribe_book({symbol});

    // Drain events before exit (approx. 2 seconds)
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    gateway.metrics().dump("Final Order Book Metrics", std::cout);

    WK_WARN("[wirekrak-lite] Experiment finished!");

    return 0;
}
