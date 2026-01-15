#pragma once

namespace wirekrak::lite {

// Semantic versioning for the Lite API
inline constexpr int version_major = 1;
inline constexpr int version_minor = 0;
inline constexpr int version_patch = 0;

// Convenience
inline constexpr bool is_v1 = (version_major == 1);

} // namespace wirekrak::lite
