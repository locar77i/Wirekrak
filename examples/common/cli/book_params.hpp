#pragma once

#include <string>
#include <cstdint>
#include <cstdlib>
#include <array>
#include <ostream>

#include <CLI/CLI.hpp>

#include "common/logger.hpp"
#include "common/cli/validators.hpp"


namespace wirekrak::examples::cli::book {

    // -------------------------------------------------------------
    // Common example parameters
    // -------------------------------------------------------------
    struct Params {
        std::string url                  = "wss://ws.kraken.com/v2";
        std::vector<std::string> symbols = {"BTC/USD"};
        std::uint32_t depth              = 10;
        bool snapshot                    = true;
        std::string log_level            = "info";

        inline void dump(const std::string& header, std::ostream& os) const {
            os << header << ":\n"
               << "  URL       : " << url << "\n"
               << "  Symbols   : ";
            for (const auto& s : symbols) { os << s << " "; }
            os << "\n"
               << "  Depth     : " << depth << "\n"
               << "  Snapshot  : " << (snapshot ? "true" : "false") << "\n"
               << "  Log Level : " << log_level << "\n";
        }
    };

    // -------------------------------------------------------------
    // Build CLI for examples
    // -------------------------------------------------------------
    [[nodiscard]]
    inline Params configure(int argc, char** argv, std::string_view description) {
        CLI::App app{std::string(description)};
        Params params{};
        app.add_option("--url", params.url, "Kraken WebSocket URL")->check(ws_url_validator)->default_val(params.url);
        app.add_option("-s,--symbol", params.symbols, "Trading symbol(s) (e.g. -s BTC/USD)")->check(symbol_validator)->default_val(params.symbols);
        app.add_option("-d,--depth", params.depth, "Order book depth (10, 25, 100, 500, 1000)")->check(depth_validator)->default_val(params.depth);
        app.add_flag("--snapshot", params.snapshot, "Request book snapshot");
        app.add_option("-l, --log-level", params.log_level, "Log level: trace | debug | info | warn | error")->default_val(params.log_level);
        app.footer(
            "This example runs indefinitely until interrupted.\n"
            "Press Ctrl+C to unsubscribe and exit cleanly.\n"
            "Let's enjoy trading with Wirekrak & Flashstrike!"
        );
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            app.exit(e, std::cout, std::cerr);
            std::exit(EXIT_FAILURE); // explicit, correct
        }
        // -------------------------------------------------------------
        // Logging
        // -------------------------------------------------------------
        set_log_level(params.log_level);
        return params;
    }


} // namespace wirekrak::examples::cli::book
