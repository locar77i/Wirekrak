#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <ostream>

#include <CLI/CLI.hpp>

namespace wirekrak::examples::cli {

    // -------------------------------------------------------------
    // Common example parameters
    // -------------------------------------------------------------
    struct Params {
        std::string url       = "wss://ws.kraken.com/v2";
        std::string symbol    = "BTC/USD";
        std::uint32_t depth   = 10;
        bool snapshot         = true;
        std::string log_level = "info";

        inline lcr::log::Level get_log_level() const {
            if (log_level == "trace")      return lcr::log::Level::Trace;
            else if (log_level == "debug") return lcr::log::Level::Debug;
            else if (log_level == "warn")  return lcr::log::Level::Warn;
            else if (log_level == "error") return lcr::log::Level::Error;
            else                           return lcr::log::Level::Info;
        }

        inline void dump(const std::string& header, std::ostream& os) const {
            os << header << ":\n"
                      << "  URL       : " << url << "\n"
                      << "  Symbol    : " << symbol << "\n"
                      << "  Depth     : " << depth << "\n"
                      << "  Snapshot  : " << (snapshot ? "true" : "false") << "\n"
                      << "  Log Level : " << log_level << "\n";
        }
    };

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

    // -------------------------------------------------------------
    // Build CLI for examples
    // -------------------------------------------------------------
    inline void configure(CLI::App& app, Params& params) {

        app.add_option("--url", params.url, "Kraken WebSocket URL")->check(ws_url_validator)->default_val(params.url);
        app.add_option("-s,--symbol", params.symbol, "Trading symbol(s) (e.g. -s BTC/USD)")->check(instrument_validator)->default_val(params.symbol);
        app.add_option("-d,--depth", params.depth, "Order book depth (10, 25, 100, 500, 1000)")->check(depth_validator)->default_val(params.depth);
        app.add_flag("--snapshot", params.snapshot, "Request book snapshot");
        app.add_option("-l, --log-level", params.log_level, "Log level: trace | debug | info | warn | error")->default_val(params.log_level);
        app.footer(
            "This example runs indefinitely until interrupted.\n"
            "Press Ctrl+C to unsubscribe and exit cleanly.\n"
            "Let's enjoy trading with WireKrak & Flashstrike!"
        );
    }

} // namespace wirekrak::examples::cli
