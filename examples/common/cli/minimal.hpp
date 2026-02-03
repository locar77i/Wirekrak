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
    std::string log_level            = "info";

    inline void dump(const std::string& header, std::ostream& os) const {
        os << "\n" << header << "\n";
        os << "  URL       : " << url << "\n";
        os << "  Log Level : " << log_level << "\n\n";
    }
};

[[nodiscard]]
inline Params configure(int argc, char** argv, std::string_view description, std::string_view footer) {
    CLI::App app{std::string(description)};
    Params params{};

    app.add_option("--url", params.url, "WebSocket endpoint")->check(ws_url_validator)->default_val(params.url);
    app.add_option("-l,--log-level", params.log_level, "Log level: trace | debug | info | warn | error")->default_val(params.log_level);

    app.footer(std::string(footer));

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
