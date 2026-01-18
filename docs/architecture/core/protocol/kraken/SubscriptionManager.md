# Subscription Manager Documentation

This document describes and includes the implementation of the **Kraken channel subscription manager** used in WireKrak.  
The manager is responsible for tracking subscription and unsubscription lifecycle state across reconnects in a deterministic and safe manner.

---

## Overview

The `Manager` class tracks all outbound subscribe and unsubscribe requests and their transitions:

```
(initial state)
    ↓ (on subscribe request)
pending_subscriptions (waiting for ACK)
    ↓ (on ACK)
active subscriptions
    ↓ (on unsubscribe request)
pending_unsubscriptions (waiting for ACK)
    ↓ (on ACK)
active subscriptions (removed)
```

On reconnect, **only active subscriptions are replayed**.

---

## Key Responsibilities

- Track pending subscribe/unsubscribe requests by request ID
- Transition subscriptions to active state only after ACK
- Safely handle partial ACKs (Kraken allows multiple symbols per request)
- Maintain deterministic state across reconnects
- Provide observability via structured logging

---

## Core Implementation

```cpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>

#include "wirekrak/core/protocol/kraken/enums.hpp"
#include "wirekrak/core/symbol/intern.hpp"
#include "lcr/log/logger.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace channel {

class Manager {
public:
    class SymbolGroup {
    public:
        struct SymbolEntry {
            SymbolId symbol_id;
            uint64_t group_id;
        };

        std::vector<SymbolEntry> entries;

        void erase(SymbolId symbol_id) {
            entries.erase(
                std::remove_if(entries.begin(), entries.end(),
                    [&](const SymbolEntry& e){ return e.symbol_id == symbol_id; }),
                entries.end()
            );
        }

        bool empty() const { return entries.empty(); }
    };

public:
    void register_subscription(std::vector<Symbol> symbols, uint64_t req_id);
    void register_unsubscription(std::vector<Symbol> symbols, uint64_t req_id);

    void process_subscribe_ack(Channel channel, uint64_t req_id, Symbol symbol, bool success);
    void process_unsubscribe_ack(Channel channel, uint64_t req_id, Symbol symbol, bool success);

    bool has_pending() const;
    bool has_active() const;

    void clear_pending();
    void clear_all();

private:
    std::unordered_map<uint64_t, std::vector<SymbolId>> pending_subscriptions_;
    std::unordered_map<uint64_t, std::vector<SymbolId>> pending_unsubscriptions_;
    std::unordered_map<uint64_t, SymbolGroup> active_;
};

} // namespace channel
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
```

---

## Design Notes

- Kraken allows **up to 10 symbols per subscription request**, so all operations are optimized for small vectors.
- State transitions are explicit and logged for debugging.
- The manager is deterministic and safe to use across reconnects.
- Designed to integrate with future persistence or replay systems.

---

## Usage Context

This component is used internally by the Kraken protocol layer to ensure:
- No duplicate subscriptions
- Correct resubscription after reconnect
- Accurate tracking of active market data streams

---

⬅️ [Back to README](../../../../ARCHITECTURE.md#subscription-manager)
