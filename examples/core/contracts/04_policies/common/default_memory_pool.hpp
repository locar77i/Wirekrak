#pragma once

#include "lcr/memory/block_pool.hpp"


namespace wirekrak::examples {
// -------------------------------------------------------------------------
// Global memory block pool
// -------------------------------------------------------------------------
constexpr static std::size_t BLOCK_SIZE = 128 * 1024; // 128 KiB
constexpr static std::size_t BLOCK_COUNT = 16;        // Number of blocks in the pool
static lcr::memory::block_pool default_memory_pool(BLOCK_SIZE, BLOCK_COUNT);

} // namespace wirekrak::examples
