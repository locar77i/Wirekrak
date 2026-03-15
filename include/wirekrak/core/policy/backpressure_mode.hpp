#pragma once


namespace wirekrak::core::policy {

// ============================================================================
// Backpressure Mode
// ============================================================================
//
// Transport detects saturation.
// Policy only classifies behavior timing.
// Transport executes mechanics.
//
// ZeroTolerance -> signal immediately and force close
// Strict        -> signal immediately and let session decide fate
// Relaxed       -> tolerate temporarily before signal to let session decide fate
// Custom        -> user-defined behavior before signal to let session decide fate
// ============================================================================

enum class BackpressureMode {
    ZeroTolerance,
    Strict,
    Relaxed,
    Custom
};

} // namespace wirekrak::core::policy
