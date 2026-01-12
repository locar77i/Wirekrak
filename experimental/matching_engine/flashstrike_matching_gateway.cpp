#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iostream>

#include <CLI/CLI.hpp>

#include "flashstrike/globals.hpp"
#include "flashstrike/matching_engine/manager.hpp"

#include "wirekrak/client.hpp"

#include "common/cli/book_params.hpp"
namespace cli = wirekrak::examples::cli;

namespace kraken = wirekrak::protocol::kraken;
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
        : metrics_()
        , engine_(max_orders, fs::get_instrument_by_name(instrument_name), target_num_partitions, metrics_)
    {
    }

    void on_book(const kraken::schema::book::Book& book) {
        fs::Trades trade_count;
        fs::Price last_price;
        fs::OrderIdx order_idx;

        for (const auto& bid : book.bids) {
            process_level_<fs::Side::BID>(bid, trade_count, last_price, order_idx);
        }

        for (const auto& ask : book.asks) {
            process_level_<fs::Side::ASK>(ask, trade_count, last_price, order_idx);
        }
    }

    void drain_trades() {
        fs::TradeEvent ev;
        while (engine_.trades_ring().pop(ev)) {
            const double price = engine_.instrument().denormalize_price(ev.price);
            const double qty   = engine_.instrument().denormalize_quantity(ev.qty);

            last_price_ = price;
            volume_    += qty;
            ++trades_;
        }
    }

    void stats_dump() const {
        WK_INFO("[FME] Trades: " << trades_ << ", Last Price: " << last_price_ << ", Volume: " << volume_);
    }

private:
    template<fs::Side S>
    inline void process_level_(const kraken::schema::book::Level& lvl, fs::Trades& trade_count, fs::Price& last_price, fs::OrderIdx& order_idx) {
        fme::Order order{};
        generate_order_<S>(order,
            engine_.normalize_price(lvl.price),
            engine_.normalize_quantity(lvl.qty)
        );

        if (order.qty == 0) {
            ++omitted_orders_;
            return;
        }

        (void)engine_.process_order<S>(order, trade_count, last_price, order_idx);
        increment_();
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
    const auto& params = cli::book::configure(argc, argv,
        "This example show you how to integrate Flashstrike Matching Engine with Wirekrak Kraken WebSocket API v2.\n"
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

    WK_DEBUG("[ME] Initializing wirekrak::WinClient...");
    wirekrak::WinClient client;

    // Register pong handler
    client.on_pong([&](const kraken::schema::system::Pong& pong) { WK_INFO(" -> " << pong.str() << ""); });

    // Register status handler
    client.on_status([&](const kraken::schema::status::Update& update) { WK_INFO(" -> " << update.str() << ""); });

    // Register regection handler
    client.on_rejection([&](const kraken::schema::rejection::Notice& notice) {  WK_WARN(" -> " << notice.str() << "");  });

    // Connect to Kraken WebSocket API v2
    if (!client.connect(params.url)) {
        return -1;
    }

    // Subscribe to book updates
    client.subscribe(kraken::schema::book::Subscribe{.symbols = {symbol}, .depth = params.depth, .snapshot = params.snapshot},
                     [&](const kraken::schema::book::Response& msg) { gateway.on_book(msg.book); }
    );

    // Main polling loop
    while (running.load()) {
        client.poll();                // 1) Poll Wirekrak client (required to process incoming messages)
        gateway.drain_trades();       // 2) Drain trades from matching engine
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 3) Sleep a bit to avoid busy loop
    }

    // Ctrl+C received
    client.unsubscribe(kraken::schema::book::Unsubscribe{.symbols = {symbol}, .depth = params.depth});

    // Drain events for 2 seconds approx.
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    WK_WARN("Experiment finished!");

    return 0;
}




