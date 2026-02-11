#include <string>
#include <cstdint>

// ----------------------------------------------------------------------------
// Helper JSON ACKs (adapt to your exact schema format if needed)
// ----------------------------------------------------------------------------

namespace json::ack {

static std::string trade_sub(std::uint64_t req_id, const std::string& symbol) {
    return R"({"method":"subscribe","success":true,"req_id":)" +
           std::to_string(req_id) +
           R"(,"result":{"channel":"trade","symbol":")" +
           symbol + R"("}})";
}

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

static std::string trade_unsub(std::uint64_t req_id, const std::string& symbol) {
    return R"({"method":"unsubscribe","success":true,"req_id":)" +
           std::to_string(req_id) +
           R"(,"result":{"channel":"trade","symbol":")" +
           symbol + R"("}})";
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

} // namespace json::ack
