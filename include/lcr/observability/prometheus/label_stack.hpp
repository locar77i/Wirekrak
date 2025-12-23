#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <cassert>


namespace lcr {
namespace observability {

static inline std::string_view to_string(uint64_t v, char* buf) noexcept {
    char* p = buf + 20; // enough for 64-bit integer (max 20 digits)
    *p = '\0';
    char* start = p;

    do {
        *--p = char('0' + (v % 10));
        v /= 10;
    } while (v > 0);

    return std::string_view(p, start - p);
}

static inline std::string_view to_string(double v, char* buf) noexcept {
    int len = snprintf(buf, 32, "%.6g", v);
    return std::string_view(buf, len);
}


class label_stack {
public:
    label_stack(size_t buffer_size = 512, size_t labels_size = 32) {
        buffer_.reserve(buffer_size);
        labels_start_.reserve(labels_size);
        buffer_ = "{}"; // start with empty labels
        pos_ = 1;       // insertion point right after '{'
    }

    inline void push(std::string_view key, std::string_view value) {
        // Insert comma if not first
        if (!labels_start_.empty()) {
            buffer_.insert(pos_, ", ", 2);
            pos_ += 2;
        }
        // Save start position
        labels_start_.push_back(pos_);
        // key
        buffer_.insert(pos_, key.data(), key.size());
        pos_ += key.size();
        // =" 
        buffer_.insert(pos_, "=\"", 2);
        pos_ += 2;
        // value
        buffer_.insert(pos_, value.data(), value.size());
        pos_ += value.size();
        // closing quote
        buffer_.insert(pos_, "\"", 1);
        pos_ += 1;
    }

    // --- Unsigned ---
    inline void push(std::string_view key, uint64_t value) {
        auto sv = to_string(value, tmp_);
        push(key, sv);
    }
    inline void push(std::string_view key, uint32_t value)       { push(key, uint64_t(value)); }

    // --- Signed ---
    inline void push(std::string_view key, int64_t value) {
        if (value < 0) {
            tmp_[0] = '-';
            auto sv = to_string(uint64_t(-value), tmp_ + 1);
            push(key, std::string_view(tmp_, 1 + sv.size()));
        } else {
            push(key, uint64_t(value));
        }
    }
    inline void push(std::string_view key, int32_t value) { push(key, int64_t(value)); }

    // --- Double ---
    inline void push(std::string_view key, double value) {
        auto sv = to_string(value, tmp_);
        push(key, sv);
    }

    inline void pop() {
        if (labels_start_.empty())
            return;
        // Get start position of the last label to remove and pop it from stack
        size_t start = labels_start_.back();
        labels_start_.pop_back();
        // Current end position
        size_t end = pos_;
        // Remove comma before label (except first)
        if (!labels_start_.empty()) {
            start -= 2;
        }
        // Erase label from buffer
        buffer_.erase(start, end - start);
        pos_ = start; // reset insertion point
    }

    inline std::string_view top() const noexcept {
        assert((pos_ <= buffer_.size()) && "Invalid position in label_stack");
        if (labels_start_.empty()) return {};
        size_t start = labels_start_.back();
        return std::string_view(buffer_.data() + start, pos_ - start);
    }

    inline void clear() noexcept {
        buffer_.clear();
        buffer_ = "{}";
        pos_ = 1;
        labels_start_.clear();
    }

    inline bool empty() const noexcept { // Empty stack have "{}"
        return labels_start_.empty();
    }

    inline const std::string& str() const noexcept {
        return buffer_;
    }

private:
    std::string buffer_;
    size_t pos_;                       // current insertion position
    std::vector<size_t> labels_start_; // stack of label start positions
    char tmp_[32];                     // temp buffer for numeric conversions
};

} // namespace observability
} // namespace lcr
