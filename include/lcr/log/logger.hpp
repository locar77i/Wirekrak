#pragma once

#include <mutex>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>

namespace lcr {
namespace log {

// ---------------------------------------------------------
// Log level
// ---------------------------------------------------------
enum class Level : uint8_t {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

// ---------------------------------------------------------
// Thread-safe global logger
// ---------------------------------------------------------
class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(Level lvl) noexcept { level_ = lvl; }

    Level level() const noexcept { return level_; }

    // Enable or disable colored output
    void enable_color(bool on) noexcept { color_enabled_ = on; }

    // Thread-safe sink setter (stdout by default)
    void set_output(std::ostream* os) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        out_ = os;
    }

    // ---------------------------------------------------------
    // Core logging function (thread-safe)
    // ---------------------------------------------------------
    void log(Level lvl, const std::string& msg) {
        if (lvl < level_) return;
        std::lock_guard<std::mutex> lock(mutex_);
        auto& os = *out_;
        if (color_enabled_) os << color_code(lvl);
        os << timestamp() << " [" << level_name(lvl) << "] " << msg;
        if (color_enabled_) os << "\033[0m"; // reset
        os << std::endl;
    }

private:
    Logger()
        : out_(&std::cout),
          level_(Level::Trace),
          color_enabled_(true) // you can default false if preferred
    {}

    // Human-readable severity names
    static const char* level_name(Level lvl) {
        switch (lvl) {
            case Level::Trace: return "TRACE";
            case Level::Debug: return "DEBUG";
            case Level::Info:  return "INFO";
            case Level::Warn:  return "WARN";
            case Level::Error: return "ERROR";
            case Level::Fatal: return "FATAL";
        }
        return "?????";
    }

    // ANSI color mappings
    static constexpr const char* color_code(Level lvl) {
        switch (lvl) {
            case Level::Trace: return "\033[37m";     // light gray
            case Level::Debug: return "\033[36m";     // cyan
            case Level::Info:  return "\033[32m";     // green
            case Level::Warn:  return "\033[33m";     // yellow
            case Level::Error: return "\033[31m";     // red
            case Level::Fatal: return "\033[1;31m";   // bold bright red
        }
        return "\033[0m";
    }

    // Timestamp generation
    static std::string timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        std::tm tm{};
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);

        return buf;
    }

private:
    std::ostream* out_;
    Level level_;
    bool color_enabled_;
    std::mutex mutex_;
};

// ---------------------------------------------------------
// Streaming log wrapper (collects << into a string)
// ---------------------------------------------------------
class LogStream {
public:
    LogStream(Level lvl) : lvl_(lvl) {}

    template<typename T>
    LogStream& operator<<(const T& v) {
        ss_ << v;
        return *this;
    }

    ~LogStream() {
        Logger::instance().log(lvl_, ss_.str());
    }

private:
    Level lvl_;
    std::ostringstream ss_;
};

} // namespace log
} // namespace lcr


// ---------------------------------------------------------
// Macros for easy logging
// ---------------------------------------------------------
#define WK_LOG_LEVEL(lvl) ::lcr::log::LogStream((lvl))

#define WK_TRACE(msg)  WK_LOG_LEVEL(::lcr::log::Level::Trace) << msg
#define WK_DEBUG(msg)  WK_LOG_LEVEL(::lcr::log::Level::Debug) << msg
#define WK_INFO(msg)   WK_LOG_LEVEL(::lcr::log::Level::Info)  << msg
#define WK_WARN(msg)   WK_LOG_LEVEL(::lcr::log::Level::Warn)  << msg
#define WK_ERROR(msg)  WK_LOG_LEVEL(::lcr::log::Level::Error) << msg
#define WK_FATAL(msg)  WK_LOG_LEVEL(::lcr::log::Level::Fatal) << msg
