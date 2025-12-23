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





// --------------------------------------------------------------------------------
// Gateway class: handles order book updates and feeds them to the matching engine
// --------------------------------------------------------------------------------

namespace wireflash {

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

} // namespace wireflash


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


int main(int argc, char** argv)
{
    using namespace lcr::log;

    WK_WARN("===  Wirekrak Kraken Book + Flashstrike Matching Engine Example ===");

    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------
    // CLI parsing
    // -------------------------------------------------------------
    CLI::App app{"This example show you how to integrate Flashstrike Matching Engine with Wirekrak Kraken WebSocket API v2.\n"};

    std::string url       = "wss://ws.kraken.com/v2";
    std::string symbol    = "BTC/USD";
    std::uint32_t depth   = 10;
    bool snapshot         = false;
    std::string log_level = "info";

    // WebSocket URL validator
    auto ws_url_validator = CLI::Validator(
        [](std::string &value) -> std::string {
            if (value.rfind("ws://", 0) == 0 || value.rfind("wss://", 0) == 0) {
                return {}; // OK
            }
            return "URL must start with ws:// or wss://";
        },
        "WebSocket URL validator"
    );

    // Instrument validator
    static constexpr std::array<std::string_view, 6> valid_instruments = {
        "BTC/USD", "ETH/USD", "SOL/USD", "LTC/USD", "XRP/USD", "DOGE/USD"
    };
    auto instrument_validator = CLI::Validator(
        [](std::string& value) -> std::string {
            for (auto v : valid_instruments) {
                if (value == v) {
                    return {};
                }
            }
            return "Instrument must be one of: BTC/USD, ETH/USD, SOL/USD, LTC/USD, XRP/USD, DOGE/USD";
        },
        "Instrument validator"
    );

    // Depth validator
    auto depth_validator = CLI::Validator(
        [](std::string& value) -> std::string {
            try {
                auto depth = std::stoul(value);
                switch (depth) {
                    case 10:
                    case 25:
                    case 100:
                    case 500:
                    case 1000:
                        return {}; // valid
                    default:
                        return "Depth must be one of: 10, 25, 100, 500, 1000";
                }
            } catch (...) {
                return "Depth must be a valid integer";
            }
        },
        "Order book depth validator"
    );

    app.add_option("--url", url, "Kraken WebSocket URL")->check(ws_url_validator)->default_val(url);
    app.add_option("-s,--symbol", symbol, "Trading symbol(s) (e.g. -s BTC/USD)")->check(instrument_validator)->default_val(symbol);
    app.add_option("-d,--depth", depth, "Order book depth (10, 25, 100, 500, 1000)")->check(depth_validator)->default_val(depth);
    app.add_flag("--snapshot", snapshot, "Request book snapshot");
    app.add_option("-l, --log-level", log_level, "Log level: trace | debug | info | warn | error")->default_val(log_level);
    app.footer(
        "This example runs indefinitely until interrupted.\n"
        "Press Ctrl+C to unsubscribe and exit cleanly.\n"
        "Let's enjoy trading with WireKrak & Flashstrike!"
    );

    CLI11_PARSE(app, argc, argv);

    // -------------------------------------------------------------
    // Logging
    // -------------------------------------------------------------
    if (log_level == "trace") Logger::instance().set_level(Level::Trace);
    else if (log_level == "debug") Logger::instance().set_level(Level::Debug);
    else if (log_level == "warn")  Logger::instance().set_level(Level::Warn);
    else if (log_level == "error") Logger::instance().set_level(Level::Error);
    else                           Logger::instance().set_level(Level::Info);

    std::cout << "=== WireKrak & Flashstrike Example ===\n"
              << "URL      : " << url << "\n"
              << "Symbol   : " << symbol << "\n"
              << "Depth    : " << depth << "\n"
              << "Snapshot : " << (snapshot ? "true" : "false") << "\n"
              << "Press Ctrl+C to exit\n\n";

    // -------------------------------------------------------------
    // Gateway setup
    // -------------------------------------------------------------
    WK_DEBUG("[ME] Initializing wireflash::Gateway...");
    wireflash::Gateway gateway(symbol);

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
    if (!client.connect(url)) {
        return -1;
    }

    // Subscribe to book updates
    client.subscribe(wpk::book::Subscribe{.symbols = {symbol}, .depth = depth, .snapshot = snapshot},
                     [&](const wpk::book::Response& msg) { gateway.on_book(msg.book); }
    );

    // Main polling loop
    while (running.load()) {
        client.poll();                // 1) Poll WireKrak client (required to process incoming messages)
        gateway.drain_trades();       // 2) Drain trades from matching engine
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 3) Sleep a bit to avoid busy loop
    }

    // Ctrl+C received
    client.unsubscribe(wpk::book::Unsubscribe{.symbols = {symbol}, .depth = depth});

    // Drain events for 2 seconds approx.
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    WK_WARN("Experiment finished!");

    return 0;
}




