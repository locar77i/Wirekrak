#pragma once

/************************************************************************************
 *                                                                                  *
 *  Symbol Interning System                                                         *
 *                                                                                  *
 *  Provides a global, thread-safe symbol interning mechanism for efficient         *
 *  storage and comparison of symbol strings. Symbols are immutable and identified  *
 *  by a unique integer ID (SymbolId). The system ensures that each unique symbol   *
 *  string is stored only once in memory, allowing for fast comparisons and reduced *
 *  memory usage.                                                                   *
 *                                                                                  *
 ************************************************************************************/

#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "wirekrak/core/symbol.hpp"


namespace wirekrak::core {

namespace symbol {

// ============================================================================
// Custom string_view hasher and comparator (content-based)
// ============================================================================
struct SvHasher {
    std::size_t operator()(std::string_view sv) const noexcept {
        std::size_t h = 146527;
        for (unsigned char c : sv)
            h = (h * 16777619) ^ c;  // FNV-1a-ish variant
        return h;
    }
};

struct SvEqual {
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        return a == b; // content compare (correct)
    }
};

// ============================================================================
// Symbol Interning System
// ============================================================================
template<std::size_t MaxSymbols>
class InternTable {
public:
    [[nodiscard]] static inline InternTable& instance() {
        static InternTable inst;
        return inst;
    }

    // Intern a symbol string → returns its stable SymbolId
    [[nodiscard]] inline SymbolId intern(std::string_view sv) {
        // --- Fast path: lookup with shared lock ---
        {
            std::shared_lock read_lock(mutex_);
            auto it = map_.find(sv);
            if (it != map_.end())
                return it->second;
        }

        // --- Slow path: exclusive lock ---
        std::unique_lock write_lock(mutex_);

        // Double-check after upgrade
        auto it = map_.find(sv);
        if (it != map_.end())
            return it->second;

        if (symbols_.size() >= MaxSymbols) {
            LCR_TRAP("symbol table capacity exceeded");
        }
        // --- Insert new symbol ---
        SymbolId id = static_cast<SymbolId>(symbols_.size());
        symbols_.emplace_back(sv);                 // permanent storage
        std::string_view view = symbols_.back();   // stable view

        map_.emplace(view, id);
        return id;
    }

    // Lookup name by ID
    [[nodiscard]] inline std::string_view name(SymbolId id) const noexcept {
        if (id >= symbols_.size())
            return {};
        return symbols_[id];
    }

    // Number of interned symbols (for debugging)
    [[nodiscard]] inline size_t count() const noexcept {
        std::shared_lock lock(mutex_);
        return symbols_.size();
    }

private:
    InternTable() {
        symbols_.reserve(MaxSymbols);  // TODO: Fix bug on vector reallocation that invalidates string_view keys when symbols > initial reserve 
        map_.reserve(MaxSymbols);
    }

private:
    mutable std::shared_mutex mutex_;
    std::vector<Symbol> symbols_; // permanent storage of symbol names
    std::unordered_map<std::string_view, SymbolId, SvHasher, SvEqual> map_;
};

} // namespace symbol
} // namespace wirekrak::core


// ============================================================================
// Public API (inline, header-only)
// ============================================================================
namespace wirekrak::core {

using SymbolTable = symbol::InternTable<4096>;

[[nodiscard]] inline SymbolId intern_symbol(std::string_view s) {
    return SymbolTable::instance().intern(s);
}

[[nodiscard]] inline std::string_view symbol_name(SymbolId id) {
    return SymbolTable::instance().name(id);
}

} // namespace wirekrak::core
