#pragma once


namespace wirekrak::core::policy {

// ============================================================================
// Retry Mode
// ============================================================================

enum class RetryMode {
    Never,
    RetryableOnly,
    Always
};

} // namespace wirekrak::core::policy
