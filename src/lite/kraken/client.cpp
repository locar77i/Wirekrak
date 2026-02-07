#include "wirekrak/lite/client.hpp"

// ---- Core includes (PRIVATE) ----
#include "wirekrak/core/transport/winhttp/websocket.hpp"
#include "wirekrak/core/protocol/kraken/session.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response_view.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/response.hpp"

namespace wirekrak::lite {

namespace kraken = wirekrak::core::protocol::kraken;
namespace schema = kraken::schema;

using WS = wirekrak::core::transport::winhttp::WebSocket;

// -----------------------------
// Impl
// -----------------------------

struct Client::Impl {
    client_config cfg;

    // Core client
    kraken::Session<WS> session;

    // Lite state
    error_handler error_cb;

    Impl(client_config cfg)
        : cfg(cfg)
        , session()
    {
        // --- Wire core status / rejection hooks ---
        session.on_rejection([this](const auto& notice) {
        if (error_cb) {
            error_cb(Error{
            ErrorCode::Rejected,
            notice.error
            });
        }
        });
        session.on_status([this](const auto& status) {
        // optional: expose status later
        });
    }

    bool connect() {
        return session.connect(cfg.endpoint);
    }

    void poll() {
        (void)session.poll();
    }
};

// -----------------------------
// Client methods
// -----------------------------

Client::Client(client_config cfg)
    : impl_(std::make_unique<Impl>(cfg)) {}

Client::Client(std::string endpoint)
    : Client(client_config{ .endpoint = std::move(endpoint) }) {}

Client::~Client() = default;

bool Client::connect() {
    return impl_->connect();
}

void Client::disconnect() {
    impl_.reset();
}

void Client::poll() {
    impl_->poll();
}

void Client::on_error(error_handler cb) {
    impl_->error_cb = std::move(cb);
}

// -----------------------------
// Trade subscriptions
// -----------------------------

void Client::subscribe_trades(std::vector<std::string> symbols, trade_handler cb, bool snapshot) {
    impl_->session.subscribe(schema::trade::Subscribe{ .symbols  = symbols, .snapshot = snapshot },
        [cb = std::move(cb)](const schema::trade::ResponseView& msg) {
            Tag tag = (msg.type == kraken::PayloadType::Snapshot ? Tag::Snapshot : Tag::Update);
            // Map each trade pointer from the view to a lite domain::Trade
            for (const auto* trade : msg.trades) {
              cb(domain::Trade{
                  .trade_id     = trade->trade_id,
                  .symbol       = trade->symbol,
                  .price        = trade->price,
                  .quantity     = trade->qty,
                  .taker_side   = (trade->side == kraken::Side::Buy) ? Side::Buy : Side::Sell,
                  .timestamp_ns = static_cast<std::uint64_t>(trade->timestamp.time_since_epoch().count()),
                  .order_type   = trade->ord_type.has() ? std::optional<std::string>{to_string(trade->ord_type.value())} : std::nullopt,
                  .tag          = tag
              });
            }
        }
    );
}

void Client::unsubscribe_trades(std::vector<std::string> symbols) {
    impl_->session.unsubscribe(schema::trade::Unsubscribe{ .symbols = symbols });
}


// -----------------------------
// Book subscriptions
// -----------------------------

void Client::subscribe_book(std::vector<std::string> symbols, book_handler cb, bool snapshot) {
    impl_->session.subscribe(schema::book::Subscribe{ .symbols  = std::move(symbols), .snapshot = snapshot },
        [cb = std::move(cb)](const kraken::schema::book::Response& resp) {
            const auto tag = (resp.type == kraken::PayloadType::Snapshot) ? Tag::Snapshot : Tag::Update;
            const std::optional<std::uint64_t> ts_ns =
                resp.book.timestamp.has() ? std::optional<std::uint64_t>{resp.book.timestamp.value().time_since_epoch().count()} : std::nullopt;

            const auto emit_levels = [&](const auto& levels, Side book_side) {
                for (const auto& lvl : levels) {
                    cb(domain::BookLevel{
                        .symbol       = resp.book.symbol,
                        .book_side    = book_side,
                        .price        = lvl.price,
                        .quantity     = lvl.qty,
                        .timestamp_ns = ts_ns,
                        .tag          = tag
                    });
                }
            };

            // Asks → sell
            emit_levels(resp.book.asks, Side::Sell);

            // Bids → buy
            emit_levels(resp.book.bids, Side::Buy);
        }
    );
}

void Client::unsubscribe_book(std::vector<std::string> symbols) {
    impl_->session.unsubscribe(schema::book::Unsubscribe{ .symbols = std::move(symbols) });
}

} // namespace wirekrak::lite
