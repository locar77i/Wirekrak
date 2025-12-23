#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iostream>

#include "wirekrak/client.hpp"

#include "flashstrike/globals.hpp"
#include "flashstrike/matching_engine/manager.hpp"

#include "lcr/log/logger.hpp"

namespace wpk = wirekrak::protocol::kraken;

namespace fs = flashstrike;
namespace fme = flashstrike::matching_engine;

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}





// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------




namespace wireflash {

class Gateway {
public:
    Gateway(fme::Manager& engine, fme::Telemetry& metrics)
        : engine_(engine)
        , metrics_(metrics)
    {}

    void on_book(const wpk::book::Book& book) {
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
    inline void process_level_(const wpk::book::Level& lvl, fs::Trades& trade_count, fs::Price& last_price, fs::OrderIdx& order_idx) {
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
    fme::Telemetry& metrics_;
    fme::Manager& engine_;

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

} // namespace wireflash





// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------






// Define matching engine telemetry (global for simplicity)
fme::Telemetry metrics{};
// Define some global stats for demo purposes
size_t processed_orders = 0;
size_t omitted_orders = 0;
double last_price = 0.0;
double volume = 0.0;
std::size_t trades = 0;


inline void increment() {
    ++processed_orders;
    if (processed_orders && processed_orders % 1000 == 0) {
        WK_DEBUG("[WWE] Processed " << processed_orders << " operations... (ommitted " << omitted_orders << " orders)");
        WK_INFO("[FME] Trades processed: " << trades << ", Last Price: " << last_price << ", Total Volume: " << volume);
    }
    if (processed_orders && processed_orders % 10000 == 0) {
            metrics.dump("Matching Engine", std::cout);
    }
}

inline void generate_order(fme::Order &out, fs::Side side, fs::Price price, fs::Quantity qty) {
    // Sequential order ID generator
    static lcr::sequence id_seq_{};
    // Fill order fields
    out.id      = id_seq_.next();
    out.type    = fs::OrderType::LIMIT;
    out.side    = side;
    out.price   = price;
    out.qty     = qty;
    out.filled  = 0;
}

inline void feed(fme::Manager &engine, const wpk::book::Book& book) {
    fs::Trades trade_count;
    fs::Price last_price;
    fs::OrderIdx order_idx;
    for (const auto& bid : book.bids) {
        fme::Order order{};
        generate_order(order, fs::Side::BID, engine.normalize_price(bid.price), engine.normalize_quantity(bid.qty));
        if (order.qty == 0) {
            omitted_orders++;
            continue;
        }
        auto status = engine.process_order<fs::Side::BID>(order, trade_count, last_price, order_idx);
        (void)status;
        increment();
    }
    for (const auto& ask : book.asks) {
        fme::Order order{};
        generate_order(order, fs::Side::ASK, engine.normalize_price(ask.price), engine.normalize_quantity(ask.qty));
        if (order.qty == 0) {
            omitted_orders++;
            continue;
        }
        auto status = engine.process_order<fs::Side::ASK>(order, trade_count, last_price, order_idx);
        (void)status;
        increment();
    }
}


int main()
{
    using namespace lcr::log;

    Logger::instance().set_level(Level::Debug);
    WK_WARN("===  Wirekrak Kraken Book + Flashstrike Matching Engine Example ===");
    WK_INFO("Press Ctrl+C to exit");

    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------
    // Matching Engine setup
    // -------------------------------------------------------------
    WK_DEBUG("[ME] Initializing flashstrike::matching_engine::Manager...");

    uint64_t max_orders = 1ull << 19;       // 2^19 = 524,288 orders. Ideally must be factor of 2 for best performance.
    uint32_t target_num_partitions = 256;   // number of partitions

    auto instrument = flashstrike::BTC_USD; // Global predefined instrument
    instrument.price_tick_units = 0.1;      // override for stress test: fine tick size
    instrument.price_max_units = 200'000.0; // override max price to allow wider range
    
    fme::Manager engine(max_orders, instrument, target_num_partitions, metrics);

    // -------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------

    WK_DEBUG("[ME] Initializing wirekrak::WinClient...");
    wirekrak::WinClient client;

    // Register pong handler
    client.on_pong([&](const wpk::system::Pong& pong) { WK_INFO(" -> " << pong.str() << ""); });

    // Register status handler
    client.on_status([&](const wpk::status::Update& update) { WK_INFO(" -> " << update.str() << ""); });

    // Register regection handler
    client.on_rejection([&](const wpk::rejection::Notice& notice) {  WK_WARN(" -> " << notice.str() << "");  });

    // Connect to Kraken WebSocket API v2
    std::string url = "wss://ws.kraken.com/v2";
    if (!client.connect(url)) {
        return -1;
    }

    // Subscribe to book updates
    std::uint32_t depth = 1000;
    client.subscribe(wpk::book::Subscribe{.symbols = {instrument.name}, .depth = depth, .snapshot = true},
                     [&](const wpk::book::Response& msg) { feed(engine, msg.book); }
    );

    // Main polling loop
    while (running.load()) {
        // 1) Poll WireKrak client (required to process incoming messages)
        client.poll();
        // 2) Drain trades from matching engine
        if (!engine.trades_ring().empty()) {
            fs::TradeEvent ev;
            while (engine.trades_ring().pop(ev)) {
                double price = engine.instrument().denormalize_price(ev.price);
                double qty   = engine.instrument().denormalize_quantity(ev.qty);
                // Update some global stats for demo purposes
                last_price = price;
                volume += qty;
                ++trades;
            }
        }
        // 3) Sleep a bit to avoid busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Ctrl+C received
    client.unsubscribe(wpk::book::Unsubscribe{.symbols = {instrument.name}, .depth = depth});

    // Drain events for 2 seconds approx.
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    WK_WARN("Experiment finished!");

    return 0;
}




