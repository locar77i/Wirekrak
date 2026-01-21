## 📜 Core Audience Contract <a name="audience-contract"></a>

Wirekrak Core is designed for infrastructure and systems engineers who require explicit control over protocol behavior, lifecycle, and correctness.

Core exposes exchange-specific semantics and low-level abstractions, prioritizing transparency, performance, and correctness over convenience. Users of Core are expected to reason about failure modes, lifecycle transitions, and protocol details. The responsibilities, guarantees, and evolution principles of Core are formally defined in the Core Audience Contract.

➡️ **[Audience Contract](./AUDIENCE_CONTRACT.md)**

---

## Architecture <a name="architecture"></a>

**Wirekrak Core** is the foundational infrastructure layer of the Wirekrak system.
It implements exchange protocol semantics, connection and subscription lifecycles,
and deterministic recovery behavior with explicit, low-level control.

Core exposes protocol reality directly. It favors transparency and correctness over
ergonomics, and deliberately avoids convenience abstractions, hidden behavior, or
implicit guarantees. All lifecycle transitions, failure modes, and protocol states
are explicit and observable by the user.

Core is designed to be header-only, allocation-free on hot paths, and compatible with
ultra-low-latency systems. It serves as the single source of protocol truth upon which
higher-level APIs are safely built, without constraining their evolution.

➡️ **[Core Architecture](./ARCHITECTURE.md)**

---

⬅️ [Back to README](../ARCHITECTURE.md#wirekrak-core)