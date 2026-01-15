#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>

#include "wirekrak/lite/dto/trade.hpp"
#include "wirekrak/lite/dto/book_level.hpp"
#include "wirekrak/lite/error.hpp"


namespace wirekrak::lite {

// -----------------------------
// Client config
// -----------------------------

struct client_config {
  std::string endpoint = "wss://ws.kraken.com/v2";
  std::chrono::milliseconds heartbeat_timeout{30'000};
  std::chrono::milliseconds message_timeout{30'000};
};


/*
===============================================================================
Wirekrak Lite Client — v1 API (STABLE)
===============================================================================

This client is the stable, hackathon-friendly façade over Wirekrak Core.

Lite v1 guarantees:
  - Stable DTO layouts
  - Stable callback signatures
  - No breaking changes without a major version bump
  - Explicit, readable behavior over maximal performance

Core remains free to evolve independently.

===============================================================================
*/
// -----------------------------
// Lite Client (Facade)
// -----------------------------

class Client {
public:
  using trade_handler = std::function<void(const dto::trade&)>;
  using book_handler  = std::function<void(const dto::book_level&)>;
  using error_handler = std::function<void(const error&)>;

  // Lite API have one obvious way to do the common thing, and one explicit way to do the advanced thing.
  explicit Client(std::string endpoint);
  explicit Client(client_config cfg = {});
  ~Client();

  // lifecycle
  bool connect();
  void disconnect();
  void poll();

  // error handling
  void on_error(error_handler cb);

  // -----------------------------
  // Trade subscriptions
  // -----------------------------
  void subscribe_trades(std::vector<std::string> symbols, trade_handler cb, bool snapshot = true);

  void unsubscribe_trades(std::vector<std::string> symbols);

  // -----------------------------
  // Book subscriptions
  // -----------------------------
  void subscribe_book(std::vector<std::string> symbols, book_handler cb, bool snapshot = true);

  void unsubscribe_book(std::vector<std::string> symbols);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace wirekrak::lite
