#include <thread>

#include "wirekrak/lite/client.hpp"
#include "wirekrak/lite/channel/dispatcher.hpp"

// ---- Core includes (PRIVATE) ----
#include "wirekrak/core/transport/winhttp/websocket.hpp"
#include "wirekrak/core/protocol/kraken/session.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response_view.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/response.hpp"
// TODO:
#include "wirekrak/core/protocol/kraken/response/partitioner.hpp"


namespace wirekrak::lite {

namespace kraken = wirekrak::core::protocol::kraken;
namespace schema = kraken::schema;

using WS = wirekrak::core::transport::winhttp::WebSocket;

// -----------------------------
// Impl
// -----------------------------

struct Client::Impl {
    client_config cfg;

    // Core session (owning)
    kraken::Session<WS> session;

    // Partitioner for trade responses: 1 trade::Response -> N trade::ResponseView (one per symbol)
    kraken::response::Partitioner<schema::trade::Response> trade_partitioner_;

    // Dispatchers (one per channel)
    channel::Dispatcher<schema::trade::ResponseView> trade_dispatcher;
    channel::Dispatcher<schema::book::Response>      book_dispatcher;

    // Lite state
    error_handler error_cb;

    // Reusable temp objects to avoid dynamic allocation in hot paths
    schema::rejection::Notice rejection_msg;
    schema::trade::Response trade_msg;
    schema::book::Response book_msg;

    Impl(client_config cfg)
        : cfg(cfg)
        , session()
    {
    }

    bool connect() {
        return session.connect(cfg.endpoint);
    }

    void poll() {
        (void)session.poll();
        while (session.pop_rejection(rejection_msg)) {
            // 1) Remove any callbacks associated with this symbol to prevent future invocations
            if (rejection_msg.symbol.has()) {
                trade_dispatcher.remove(rejection_msg.symbol.value());
                book_dispatcher.remove(rejection_msg.symbol.value());
            }
            // 2) Emit error callback if provided
            if (error_cb) {
                error_cb(Error{ErrorCode::Rejected, rejection_msg.error});
            }
        }

        // -------------------------------------------------
        // Drain trade messages in a loop until empty
        // to ensure we process all messages received in this poll
        // -------------------------------------------------
        while (session.pop_trade_message(trade_msg)) {
            trade_partitioner_.reset(trade_msg);
            // NOTE: ResponseView is valid only for the duration of this dispatch loop.
            // Callbacks must not store references to msg beyond the call.
            for (const auto& view : trade_partitioner_.views()) {
                trade_dispatcher.dispatch(view);
            }
        }

        // -------------------------------------------------
        // Drain book messages in a loop until empty
        // to ensure we process all messages received in this poll
        // -------------------------------------------------
        while (session.pop_book_message(book_msg)) {
            book_dispatcher.dispatch(book_msg);
        }
    }

    // -----------------------------------------------------------------------------
    // Quiescence
    // -----------------------------------------------------------------------------
    //
    // Returns true if the Lite client is idle.
    //
    // Lite-idle means:
    //   • Core has no pending protocol work (ACKs, rejections, replay)
    //   • Lite owns no active callbacks or dispatchable behavior
    //
    // This is a compositional quiescence signal used for:
    //   • graceful shutdown
    //   • drain loops
    //   • deterministic teardown
    //
    // Notes:
    //   • This does NOT imply the connection is closed
    //   • This does NOT guarantee the exchange has no subscriptions
    //   • This does NOT prevent future messages if polling continues
    //
    // Complexity: O(1)
    // -----------------------------------------------------------------------------
    bool is_idle() const {
        return // No protocol work remains AND no user-visible behavior remains
            session.is_idle() &&
            trade_dispatcher.is_idle() &&
            book_dispatcher.is_idle();
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

// -----------------------------------------------------------------------------
// Convenience execution loop — run until protocol quiescence
// -----------------------------------------------------------------------------
void Client::run_until_idle(std::chrono::milliseconds tick) {
    const bool cooperative = (tick.count() > 0);
    while (true) {
        // Drive client progress
        poll();
        // If idle, no work remains: exit
        if (is_idle()) [[unlikely]] {
            return;
        }
        // If tick > 0, sleep to avoid busy-waiting. Otherwise, immediately poll again to drive progress.
        if (cooperative) [[likely]] {
            std::this_thread::sleep_for(tick);
        }
    }
}

bool Client::is_idle() const {
    return  impl_->is_idle();
}

void Client::on_error(error_handler cb) {
    impl_->error_cb = std::move(cb);
}

// -----------------------------
// Trade subscriptions
// -----------------------------

void Client::subscribe_trades(std::vector<std::string> symbols, trade_handler cb, bool snapshot) {
    // ---------------------------------------------------------------------
    // Mapping: Kraken trade response → Lite domain::Trade
    // ---------------------------------------------------------------------
    auto emit_trades = [cb = std::move(cb)](const schema::trade::ResponseView& msg) {
        const Tag tag = (msg.type == kraken::PayloadType::Snapshot) ? Tag::Snapshot : Tag::Update;
        // Map each trade pointer from the view to a lite domain::Trade
        for (const auto* trade : msg.trades) {
            cb(domain::Trade{
                .trade_id     = trade->trade_id,
                .symbol       = trade->symbol,
                .price        = trade->price,
                .quantity     = trade->qty,
                .taker_side   = (trade->side == kraken::Side::Buy) ? Side::Buy : Side::Sell,
                .timestamp_ns = static_cast<std::uint64_t>(trade->timestamp.time_since_epoch().count()),
                .order_type   = trade->ord_type.has() ? std::optional<std::string>{ to_string(trade->ord_type.value())} : std::nullopt,
                .tag          = tag
            });
        }
    };

    // Make a copy ONLY for Core (to remove in the future)
    // Core consumes by value, Lite owns original
    auto symbols_for_core = symbols;

    // 1) Submit intent to Core
    impl_->session.subscribe(
        schema::trade::Subscribe{ .symbols = std::move(symbols_for_core), .snapshot = snapshot }
    );

    // 2) Register behavior in Lite dispatcher
    impl_->trade_dispatcher.add(std::move(symbols), std::move(emit_trades));
}

void Client::unsubscribe_trades(const std::vector<std::string>& symbols) {
    impl_->session.unsubscribe(schema::trade::Unsubscribe{ .symbols = std::move(symbols) });
    // Optional: if Kraken rejects later, rejection handling will clean this anyway
    // Lite removes behavior immediately on unsubscribe intent, not on ACK.
    impl_->trade_dispatcher.remove(symbols);
}


// -----------------------------
// Book subscriptions
// -----------------------------

void Client::subscribe_book(std::vector<std::string> symbols, book_handler cb, bool snapshot) {
    // ---------------------------------------------------------------------
    // Mapping: Kraken book response → Lite domain::BookLevel
    // ---------------------------------------------------------------------
    auto emit_book_levels = [cb = std::move(cb)](const kraken::schema::book::Response& resp) {
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
    };

    // Make a copy ONLY for Core (to remove in the future)
    // Core consumes by value, Lite owns original
    auto symbols_for_core = symbols;

    // 1) Submit intent to Core
    impl_->session.subscribe(
        schema::book::Subscribe{ .symbols  = std::move(symbols_for_core), .snapshot = snapshot }
    );

    // 2) Register behavior in Lite dispatcher
    impl_->book_dispatcher.add(std::move(symbols), std::move(emit_book_levels));
}

void Client::unsubscribe_book(const std::vector<std::string>& symbols) {
    impl_->session.unsubscribe(schema::book::Unsubscribe{ .symbols = std::move(symbols) });
    // Optional: if Kraken rejects later, rejection handling will clean this anyway
    // Lite removes behavior immediately on unsubscribe intent, not on ACK.
    impl_->book_dispatcher.remove(symbols);
}

} // namespace wirekrak::lite
