#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>

#include "wirekrak/lite/dto/trade.hpp"


namespace wirekrak::lite {

// -----------------------------
// Error model
// -----------------------------

enum class error_code {
  transport,
  protocol,
  rejected,
  disconnected
};

struct error {
  error_code code;
  std::string message;
};

// -----------------------------
// Client config
// -----------------------------

struct client_config {
  std::string endpoint = "wss://ws.kraken.com/v2";
  std::chrono::milliseconds heartbeat_timeout{30'000};
  std::chrono::milliseconds message_timeout{30'000};
};

// -----------------------------
// Lite Client (Facade)
// -----------------------------

class Client {
public:
  using trade_handler = std::function<void(const dto::trade&)>;
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

  // subscriptions
  void subscribe_trades(
    std::vector<std::string> symbols,
    trade_handler cb,
    bool snapshot = true
  );

  void unsubscribe_trades(std::vector<std::string> symbols);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace wirekrak::lite
