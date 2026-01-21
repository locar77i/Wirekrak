# Wirekrak Lite — Public API Stability

Wirekrak follows a strict API stability policy to ensure that applications built
on top of the **Lite SDK** remain reliable as the system evolves.

---

## What the Lite SDK Guarantees

The following guarantees apply **within the same major version**:

- **Stable public surface**  
  All symbols explicitly exposed under the `wirekrak::lite` namespace are part of the
  public API contract.

- **Source compatibility**  
  Public Lite headers will not change in incompatible ways within the same major version.

- **Stable domain value types**  
  Domain value types (such as `lite::Trade`, `lite::BookLevel`) preserve their semantic
  meaning and field layouts.

- **Explicit control flow**  
  The Lite SDK does not introduce hidden background threads or implicit concurrency.  
  Polling, lifecycle, and callback invocation remain under user control.

- **Exchange-neutral API surface**  
  The public Lite API does not expose exchange-specific concepts, protocols, or Core internals.  
  The underlying exchange implementation is an internal detail.

---

## What Is Explicitly *Not* Guaranteed

The following are **not** part of the public API contract and may change at any time:

- Internal namespaces and implementation details  
  (including `wirekrak::core`, transport layers, protocol schemas, and internal Lite implementation files)

- Exchange-specific behavior or defaults  
  (including default endpoints and internal adapters)

- External APIs, examples, and tooling

- Performance characteristics  
  (latency, throughput, memory usage)

- Error code completeness or categorization

Consumers must rely **only** on the public Lite headers and documented behavior for stable integration.

---

## Enum and Semantic Tag Evolution Policy

Semantic enums and tags exposed via the Lite SDK (such as `Side` and `Tag`) follow these rules:

- Existing values will **never change meaning**
- Values will **never be removed**
- New values **may be added** in future versions
- Enums and tags are **not guaranteed to be exhaustive**

Consumers **must not assume** that a switch over an enum or tag handles all possible values
and should always provide a fallback path.

---

## API Evolution and Versioning

Any change that violates the guarantees above will result in a **major version increment**.

Minor and patch releases may:
- add new functionality
- add new enum values
- extend configuration options

as long as existing Lite integrations remain source-compatible.

Internal implementation details may evolve independently as long as the public `wirekrak::lite` contract is preserved.

---

## Related Documents

➡️ **[Architecture Overview](./ARCHITECTURE.md)**

➡️ **[Audience Contract](./AUDIENCE_CONTRACT.md)**

➡️ **[Quick Usage Guide](./QUICK_USAGE.md)**

---

⬅️ [Back to README](./README.md#api-stability)
