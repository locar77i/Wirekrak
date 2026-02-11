#include <string>
#include <cstdint>

// ----------------------------------------------------------------------------
// Helper JSON ACKs (adapt to your exact schema format if needed)
// ----------------------------------------------------------------------------

namespace json::ack {

// -----------------------------------------------------------------------------
// Trade
// -----------------------------------------------------------------------------

static std::string trade_sub(std::uint64_t req_id, const std::string& symbol) {
    return R"({"method":"subscribe","success":true,"req_id":)" +
           std::to_string(req_id) +
           R"(,"result":{"channel":"trade","symbol":")" +
           symbol + R"("}})";
}

static std::string trade_unsub(std::uint64_t req_id, const std::string& symbol) {
    return R"({"method":"unsubscribe","success":true,"req_id":)" +
           std::to_string(req_id) +
           R"(,"result":{"channel":"trade","symbol":")" +
           symbol + R"("}})";
}

// -----------------------------------------------------------------------------
// Book
// -----------------------------------------------------------------------------

static std::string book_sub(std::uint64_t req_id, const std::string& symbol) {
    return R"({"method":"subscribe","success":true,"req_id":)" +
           std::to_string(req_id) +
           R"(,"result":{"channel":"book","symbol":")" +
           symbol + R"("}})";
}

static std::string book_sub(std::uint64_t req_id, const std::string& symbol, std::uint32_t depth) {
    return R"({"method":"subscribe","success":true,"req_id":)" +
           std::to_string(req_id) +
           R"(,"result":{"channel":"book","symbol":")" +
           symbol +
           R"(","depth":)" +
           std::to_string(depth) +
           R"(}})";
}

static std::string book_unsub(std::uint64_t req_id, const std::string& symbol, std::uint32_t depth) {
    return R"({"method":"unsubscribe","success":true,"req_id":)" +
           std::to_string(req_id) +
           R"(,"result":{"channel":"book","symbol":")" +
           symbol +
           R"(","depth":)" +
           std::to_string(depth) +
           R"(}})";
}

// -----------------------------------------------------------------------------
// Rejection notice
// -----------------------------------------------------------------------------

static inline std::string rejection_notice(std::string_view method, std::uint64_t req_id, const std::string& symbol, std::string_view error, bool success = false) {
    return  R"({"method":")" + std::string(method) +
            R"(","success":)" + (success ? "true" : "false") +
            R"(,"req_id":)" + std::to_string(req_id) +
            R"(,"symbol":")" + symbol +
            R"(","error":")" + std::string(error) +
            R"("})";
}

} // namespace json::ack
