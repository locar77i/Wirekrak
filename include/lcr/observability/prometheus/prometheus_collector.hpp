#pragma once

#include <string>
#include <ostream>
#include <iomanip>

#include "lcr/observability/prometheus/label_stack.hpp"


namespace lcr {
namespace observability {

class prometheus_collector {
public:
    // Constructor
    explicit prometheus_collector(std::ostringstream& os) noexcept
        : os_(os)
    {}

    inline void add_gauge(uint32_t value, const std::string& name, const std::string& help) noexcept {
        add_gauge(uint64_t(value), name, help);
    }

    inline void add_gauge(uint64_t value, const std::string& name, const std::string& help) noexcept {
        os_ << "# HELP " << name << " " << help << '\n'
             << "# TYPE " << name << " gauge\n"
             << name << labels_.str() << " " << value << '\n';
    }

    inline void add_gauge(double value, const std::string& name, const std::string& help, int precision=2, bool fixed = true) noexcept {
        os_ << "# HELP " << name << " " << help << '\n'
             << "# TYPE " << name << " gauge\n";
        if (fixed) [[likely]] {
            os_ << std::fixed;
        }
        else {
            os_ << std::defaultfloat;
        }
        os_ << name << labels_.str() << " " << std::setprecision(precision) << value << '\n';
    }

    inline void add_gauge(float value, const std::string& name, const std::string& help, int precision=2, bool fixed = true) noexcept {
        add_gauge(static_cast<double>(value), name, help, precision, fixed);
    }

    inline void add_counter(uint32_t value, const std::string& name, const std::string& help) noexcept {
        add_counter(uint64_t(value), name, help);
    }

    inline void add_counter(uint64_t value, const std::string& name, const std::string& help) noexcept {
        os_ << "# HELP " << name << " " << help << '\n'
             << "# TYPE " << name << " counter\n"
             << name << labels_.str() << " " << value << '\n';
    }

    inline void add_summary(const metrics::latency_percentiles& summary, const std::string& name, const std::string& help) noexcept {
        os_ << "# HELP " << name << " " << help << '\n'
             << "# TYPE " << name << " summary\n";
        // Create a lambda to emit each quantile line
        auto emit = [&](const char* q_label, uint64_t value_ns) {
            labels_.push("percentile", q_label);
            os_ << name << labels_.str() << " " << value_ns << "\n";
            labels_.pop();
        };
        // Emit each percentile
        emit("50",  summary.p50);
        emit("90",  summary.p90);
        emit("99",  summary.p99);
        emit("99.9", summary.p999);
        emit("99.99", summary.p9999);
        emit("99.999", summary.p99999);
        emit("99.9999", summary.p999999);
    }

    inline void push_label(const std::string& key, const std::string& value) noexcept {
        labels_.push(key, value);
    }

    inline void pop_label() noexcept {
        labels_.pop();
    }

    inline std::string str() const noexcept {
        return os_.str();
    }

private:
    std::ostringstream& os_;
    label_stack labels_{};
};

} // namespace observability
} // namespace lcr

