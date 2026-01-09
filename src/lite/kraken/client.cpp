#include "wirekrak/lite/kraken/client.hpp"

// ---- Core includes (PRIVATE) ----
#include "wirekrak/transport/winhttp/websocket.hpp"
#include "wirekrak/protocol/kraken/client.hpp"
#include "wirekrak/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/protocol/kraken/schema/trade/response.hpp"


namespace wirekrak::lite {

namespace kraken = protocol::kraken;
namespace schema = kraken::schema;

using WS = wirekrak::transport::winhttp::WebSocket;
using CoreClient = kraken::Client<WS>;

// -----------------------------
// Impl
// -----------------------------

struct Client::Impl {
  client_config cfg;

  // Core client
  CoreClient core;

  // Lite state
  error_handler error_cb;

  Impl(client_config cfg)
    : cfg(cfg)
    , core()
  {
    // --- Wire core status / rejection hooks ---
    core.on_rejection([this](const auto& notice) {
      if (error_cb) {
        error_cb(error{
          error_code::rejected,
          notice.error
        });
      }
    });
    core.on_status([this](const auto& status) {
      // optional: expose status later
    });
  }

  bool connect() {
    return core.connect(cfg.endpoint);
  }

  void poll() {
    core.poll();
  }
};

// -----------------------------
// Client methods
// -----------------------------

Client::Client(client_config cfg)
  : impl_(std::make_unique<Impl>(cfg)) {}

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
    impl_->core.subscribe(schema::trade::Subscribe{ .symbols  = symbols, .snapshot = snapshot },
        [cb = std::move(cb)](const schema::trade::Trade& msg) {
            cb(dto::trade{
                .trade_id     = msg.trade_id,
                .symbol       = msg.symbol,
                .price        = msg.price,
                .quantity     = msg.qty,
                .taker_side   = (msg.side == kraken::Side::Buy) ? side::buy : side::sell,
                .timestamp_ns = static_cast<std::uint64_t>(msg.timestamp.time_since_epoch().count()),
                .order_type = msg.ord_type.has() ? std::optional<std::string>{to_string(msg.ord_type.value())} : std::nullopt
            });
        }
    );
}

void Client::unsubscribe_trades(std::vector<std::string> symbols) {
  impl_->core.unsubscribe(schema::trade::Unsubscribe{ .symbols = symbols });
}

} // namespace wirekrak::lite
