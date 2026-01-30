#pragma once

#include <string>
#include <vector>
#include <ostream>
#include <cstdlib>

#include <CLI/CLI.hpp>

#include "common/cli/validators.hpp"
#include "common/logger.hpp"

namespace wirekrak::cli::minimal {

struct Params {
    std::string url                  = "wss://ws.kraken.com/v2";
    std::vector<std::string> symbols = {"BTC/EUR"};
    std::string log_level            = "info";

    inline void dump(const std::string& header, std::ostream& os) const {
        os << header << ":\n  URL       : " << url << "\n" << "  Symbols   : ";
        for (const auto& s : symbols) {
            os << s << " ";
        }
        os << "\n  Log Level : " << log_level << "\n";
    }
};

[[nodiscard]]
inline Params configure(int argc, char** argv, std::string_view description) {
    CLI::App app{std::string(description)};
    Params params{};

    app.add_option("--url", params.url, "WebSocket endpoint")->check(ws_url_validator)->default_val(params.url);
    app.add_option("-s,--symbol", params.symbols, "Symbol(s) to use (e.g. -s BTC/EUR)")->check(symbol_validator)->default_val(params.symbols);
    app.add_option("-l,--log-level", params.log_level, "Log level: trace | debug | info | warn | error")->default_val(params.log_level);

    app.footer(
        "This example demonstrates a Core contract.\n"
        "Behavior is observable via logs and callbacks.\n"
        "No assumptions are made beyond protocol ACKs."
    );

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        app.exit(e, std::cout, std::cerr);
        std::exit(EXIT_FAILURE);
    }

    log::set_level(params.log_level);
    return params;
}

} // namespace wirekrak::cli::minimal
