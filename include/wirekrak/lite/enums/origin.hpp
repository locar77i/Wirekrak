#pragma once

#include <string_view>


namespace wirekrak::lite {

enum class origin {
    snapshot,
    update
};

constexpr std::string_view to_string(origin o) noexcept {
    switch (o) {
        case origin::snapshot: return "snapshot";
        case origin::update:   return "update";
    }
    return "unknown";
}

} // namespace wirekrak::lite
