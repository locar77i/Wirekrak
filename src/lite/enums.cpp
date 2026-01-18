#include "wirekrak/lite/enums.hpp"


namespace wirekrak::lite {

std::string_view to_string(Side s) noexcept {
    switch (s) {
        case Side::Buy:  return "Buy";
        case Side::Sell: return "Sell";
    }
    return "unknown";
}

std::string_view to_string(Tag o) noexcept {
    switch (o) {
        case Tag::Snapshot: return "Snapshot";
        case Tag::Update:   return "Update";
    }
    return "unknown";
}

} // namespace wirekrak::lite
