#include "wirekrak/lite/enums.hpp"


namespace wirekrak::lite {

std::string_view to_string(side s) noexcept {
    switch (s) {
        case side::buy:  return "buy";
        case side::sell: return "sell";
    }
    return "unknown";
}

std::string_view to_string(origin o) noexcept {
    switch (o) {
        case origin::snapshot: return "snapshot";
        case origin::update:   return "update";
    }
    return "unknown";
}

} // namespace wirekrak::lite
