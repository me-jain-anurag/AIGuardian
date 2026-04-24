// src/logger.cpp
// Structured logging
#include "guardian/logger.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>

namespace guardian {

// ============================================================================
// Helpers
// ============================================================================

static std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    std::tm tm_val{};
#ifdef _WIN32
    gmtime_s(&tm_val, &t);
#else
    gmtime_r(&t, &tm_val);
#endif
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static std::string level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

static nlohmann::json entry_to_json(const LogEntry& e) {
    return nlohmann::json{
        {"timestamp", e.timestamp},
        {"level",     level_to_string(e.level)},
        {"component", e.component},
        {"tool_name", e.tool_name},
        {"session_id",e.session_id},
        {"message",   e.message},
        {"details",   e.details}
    };
}

// ============================================================================
// Logger Implementation
// ============================================================================

Logger::Logger(LogLevel min_level) : min_level_(min_level) {}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    switch (level) {
        case LogLevel::DEBUG: debug(component, message); break;
        case LogLevel::INFO:  info(component, message);  break;
        case LogLevel::WARN:  warn(component, message);  break;
        case LogLevel::ERROR: error(component, message); break;
    }
}

void Logger::debug(const std::string& component, const std::string& message, const std::string& details) {
    if (min_level_ > LogLevel::DEBUG) return;
    LogEntry entry{current_timestamp(), LogLevel::DEBUG, component, "", "", message, details};
    {
        std::lock_guard<std::mutex> lk(mutex_);
        entries_.push_back(entry);
    }
    if (!output_file_.empty()) {
        std::ofstream f(output_file_, std::ios::app);
        f << "[DEBUG] [" << component << "] " << message << "\n";
    }
    if (json_format_)
        std::cout << entry_to_json(entry).dump() << "\n";
    else
        std::cout << "[DEBUG] [" << component << "] " << message << "\n";
}

void Logger::info(const std::string& component, const std::string& message, const std::string& details) {
    if (min_level_ > LogLevel::INFO) return;
    LogEntry entry{current_timestamp(), LogLevel::INFO, component, "", "", message, details};
    {
        std::lock_guard<std::mutex> lk(mutex_);
        entries_.push_back(entry);
    }
    if (!output_file_.empty()) {
        std::ofstream f(output_file_, std::ios::app);
        f << "[INFO] [" << component << "] " << message << "\n";
    }
    if (json_format_)
        std::cout << entry_to_json(entry).dump() << "\n";
    else
        std::cout << "[INFO] [" << component << "] " << message << "\n";
}

void Logger::warn(const std::string& component, const std::string& message, const std::string& details) {
    if (min_level_ > LogLevel::WARN) return;
    LogEntry entry{current_timestamp(), LogLevel::WARN, component, "", "", message, details};
    {
        std::lock_guard<std::mutex> lk(mutex_);
        entries_.push_back(entry);
    }
    if (!output_file_.empty()) {
        std::ofstream f(output_file_, std::ios::app);
        f << "[WARN] [" << component << "] " << message << "\n";
    }
    if (json_format_)
        std::cout << entry_to_json(entry).dump() << "\n";
    else
        std::cout << "[WARN] [" << component << "] " << message << "\n";
}

void Logger::error(const std::string& component, const std::string& message, const std::string& details) {
    if (min_level_ > LogLevel::ERROR) return;
    LogEntry entry{current_timestamp(), LogLevel::ERROR, component, "", "", message, details};
    {
        std::lock_guard<std::mutex> lk(mutex_);
        entries_.push_back(entry);
    }
    if (!output_file_.empty()) {
        std::ofstream f(output_file_, std::ios::app);
        f << "[ERROR] [" << component << "] " << message << "\n";
    }
    if (json_format_)
        std::cerr << entry_to_json(entry).dump() << "\n";
    else
        std::cerr << "[ERROR] [" << component << "] " << message << "\n";
}

void Logger::set_level(LogLevel level) {
    min_level_ = level;
}

void Logger::set_output_file(const std::string& file_path) {
    output_file_ = file_path;
}

void Logger::set_json_format(bool enabled) {
    json_format_ = enabled;
}

std::string Logger::export_logs() const {
    std::lock_guard<std::mutex> lk(mutex_);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : entries_) {
        arr.push_back(entry_to_json(e));
    }
    return arr.dump();
}

std::string Logger::export_logs(LogLevel min_level) const {
    std::lock_guard<std::mutex> lk(mutex_);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : entries_) {
        if (e.level >= min_level) {
            arr.push_back(entry_to_json(e));
        }
    }
    return arr.dump();
}

} // namespace guardian
