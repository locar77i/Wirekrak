#pragma once

#include <array>
#include <string>
#include <string_view>
#include <regex>

#include <CLI/CLI.hpp>


namespace wirekrak::examples::cli {

// -------------------------------------------------------------
// WebSocket URL validator
// -------------------------------------------------------------
inline auto ws_url_validator = CLI::Validator(
    [](std::string& value) -> std::string {
        if (value.rfind("ws://", 0) == 0 || value.rfind("wss://", 0) == 0) {
            return {};
        }
        return "URL must start with ws:// or wss://";
    },
    "WebSocket URL validator"
);


// -------------------------------------------------------------
// Symbol validator
// -------------------------------------------------------------
inline auto symbol_validator = CLI::Validator(
    [](std::string &value) -> std::string {
        if (value.find('/') != std::string::npos) {
            return {};
        }
        return "Symbol must be in format BASE/QUOTE (e.g. BTC/USD)";
    },
    "Trading symbol validator"
);

/*
// -------------------------------------------------------------
// Instrument validator
// -------------------------------------------------------------
inline constexpr std::array<std::string_view, 6> valid_instruments = {
    "BTC/USD", "ETH/USD", "SOL/USD", "LTC/USD", "XRP/USD", "DOGE/USD"
};

inline auto instrument_validator = CLI::Validator(
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
*/


// -------------------------------------------------------------
// Order book depth validator
// -------------------------------------------------------------
inline auto depth_validator = CLI::Validator(
    [](std::string& value) -> std::string {
        try {
            auto d = std::stoul(value);
            switch (d) {
                case 10:
                case 25:
                case 100:
                case 500:
                case 1000:
                    return {};
                default:
                    return "Depth must be one of: 10, 25, 100, 500, 1000";
            }
        } catch (...) {
            return "Depth must be a valid integer";
        }
    },
    "Order book depth validator"
);

} // namespace wirekrak::examples::cli
