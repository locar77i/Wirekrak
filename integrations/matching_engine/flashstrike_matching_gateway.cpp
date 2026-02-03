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
#include "flashstrike/matching_engine/manager.hpp"
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
        : engine_(max_orders, fs::get_instrument_by_name(instrument_name), target_num_partitions, metrics_)
    {
    }

    void on_book_level(const BookLevel& lvl) {
        fs::Trades trade_count;
        fs::Price last_price;
        fs::OrderIdx order_idx;

        if (lvl.quantity == 0.0) {
            ++omitted_orders_;
            return;
        }

        if (lvl.book_side == wirekrak::lite::Side::Buy) {
            process_<fs::Side::BID>(lvl, trade_count, last_price, order_idx);
        } else {
            process_<fs::Side::ASK>(lvl, trade_count, last_price, order_idx);
        }
    }

    void drain_trades() {
        fs::TradeEvent ev;
        while (engine_.trades_ring().pop(ev)) {
            last_price_ = engine_.instrument().denormalize_price(ev.price);
            volume_    += engine_.instrument().denormalize_quantity(ev.qty);
            ++trades_;
        }
    }

private:
    fme::Telemetry metrics_;
    fme::Manager engine_;

    // demo / metrics
    std::size_t trades_ = 0;
    std::size_t processed_orders_ = 0;
    std::size_t omitted_orders_ = 0;
    double last_price_ = 0.0;
    double volume_ = 0.0;

private:
    template<fs::Side S>
    inline void process_(const BookLevel& lvl, fs::Trades& trade_count, fs::Price& last_price, fs::OrderIdx& order_idx) {
        fme::Order order{};
        generate_order_<S>(order,
            engine_.normalize_price(lvl.price),
            engine_.normalize_quantity(lvl.quantity)
        );

        (void)engine_.process_order<S>(order, trade_count, last_price, order_idx);
        increment_();
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

    inline void increment_() {
        ++processed_orders_;
        if (processed_orders_ && processed_orders_ % 1000 == 0) {
            WK_DEBUG("[WWE] Processed " << processed_orders_ << " operations... (ommitted " << omitted_orders_ << " orders)");
            WK_INFO("[FME] Trades processed: " << trades_ << ", Last Price: " << last_price_ << ", Total Volume: " << volume_);
        }
        if (processed_orders_ && processed_orders_ % 10000 == 0) {
                metrics_.dump("Matching Engine", std::cout);
        }
    }
};

} // namespace flashstrike


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


int main(int argc, char** argv)
{
    WK_WARN("===  Wirekrak Kraken Book + Flashstrike Matching Engine Example ===");

    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------
    // CLI parsing
    // -------------------------------------------------------------
    const auto& params = wirekrak::cli::book::configure(argc, argv,
        "This example show you how to integrate Flashstrike Matching Engine with Wirekrak Kraken WebSocket API v2.\n",
        "Subscribes to order book updates for a given symbol and feeds them to Flashstrike Matching Engine.\n"
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
        gateway.drain_trades();       // 2) Drain trades from matching engine
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 3) Sleep a bit to avoid busy loop
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

    WK_WARN("[wirekrak-lite] Experiment finished!");

    return 0;
}




