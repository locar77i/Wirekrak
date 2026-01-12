#pragma once

#include <string>

#include "lcr/log/logger.hpp"


namespace wirekrak::examples {

    inline void set_log_level(const std::string& log_level) {
        using namespace lcr::log;
        if (log_level == "trace")      Logger::instance().set_level(Level::Trace);
        else if (log_level == "debug") Logger::instance().set_level(Level::Debug);
        else if (log_level == "warn")  Logger::instance().set_level(Level::Warn);
        else if (log_level == "error") Logger::instance().set_level(Level::Error);
        else                           Logger::instance().set_level(Level::Info);
    }

} // namespace wirekrak::examples
