// src/logger.cpp
// Owner: Dev D (stub implementation by Dev A for integration)
// Structured logging
#include "guardian/logger.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

namespace guardian {

// ============================================================================
// Logger Implementation
// ============================================================================

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::log(LogLevel level, const std::string& component,
                  const std::string& message) {
    if (level < min_level_) return;

    LogEntry entry;
    entry.level = level;
    entry.component = component;
    entry.message = message;
    entry.timestamp = std::chrono::system_clock::now();

    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::ostringstream oss;

    if (format_ == "json") {
        oss << "{\"level\":\"" << level_to_string(level) << "\""
            << ",\"component\":\"" << component << "\""
            << ",\"message\":\"" << message << "\""
            << ",\"timestamp\":" << time_t << "}";
    } else {
        struct tm tm_buf;
#ifdef _MSC_VER
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif
        oss << "[" << std::put_time(&tm_buf, "%H:%M:%S") << "] "
            << "[" << level_to_string(level) << "] "
            << "[" << component << "] "
            << message;
    }

    std::string formatted = oss.str();

    if (level >= LogLevel::WARN) {
        std::cerr << formatted << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }
}

void Logger::set_level(LogLevel level) {
    min_level_ = level;
}

void Logger::set_format(const std::string& format) {
    format_ = format;
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

} // namespace guardian
