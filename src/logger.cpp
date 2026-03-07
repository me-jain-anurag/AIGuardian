// src/logger.cpp
// Owner: Dev D
// Structured logging with level filtering, JSON/text output, file support
#include "guardian/logger.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <stdexcept>

namespace guardian {

// ============================================================================
// Construction / Destruction
// ============================================================================

Logger::Logger(LogLevel min_level)
    : min_level_(min_level) {}

Logger::~Logger() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

// ============================================================================
// Singleton accessor (backward-compatible with guardian.cpp stubs)
// ============================================================================

Logger& Logger::instance() {
    static Logger singleton(LogLevel::INFO);
    return singleton;
}

// ============================================================================
// Convenience logging methods
// ============================================================================

void Logger::debug(const std::string& component, const std::string& message,
                   const std::string& details) {
    log_impl(LogLevel::DEBUG, component, message, "", "", details);
}

void Logger::info(const std::string& component, const std::string& message,
                  const std::string& details) {
    log_impl(LogLevel::INFO, component, message, "", "", details);
}

void Logger::warn(const std::string& component, const std::string& message,
                  const std::string& details) {
    log_impl(LogLevel::WARN, component, message, "", "", details);
}

void Logger::error(const std::string& component, const std::string& message,
                   const std::string& details) {
    log_impl(LogLevel::ERROR, component, message, "", "", details);
}

void Logger::log_with_context(LogLevel level,
                               const std::string& component,
                               const std::string& message,
                               const std::string& tool_name,
                               const std::string& session_id,
                               const std::string& details) {
    log_impl(level, component, message, tool_name, session_id, details);
}

// ============================================================================
// Core log_impl
// ============================================================================

void Logger::log_impl(LogLevel level,
                       const std::string& component,
                       const std::string& message,
                       const std::string& tool_name,
                       const std::string& session_id,
                       const std::string& details) {
    if (level < min_level_) return;

    // Build entry
    LogEntry entry;
    entry.timestamp  = format_timestamp();
    entry.level      = level;
    entry.component  = component;
    entry.tool_name  = tool_name;
    entry.session_id = session_id;
    entry.message    = message;
    entry.details    = details;

    // Format output string
    std::string formatted = json_format_
        ? entry_to_json(entry)
        : format_entry_text(entry);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(entry);

        // Console output
        if (level >= LogLevel::WARN) {
            std::cerr << formatted << "\n";
        } else {
            std::cout << formatted << "\n";
        }

        // File output (fail-safe: if file write fails, log to stderr and continue)
        if (file_stream_.is_open()) {
            try {
                file_stream_ << formatted << "\n";
                file_stream_.flush();
            } catch (const std::exception& ex) {
                std::cerr << "[ERROR] [Logger] Failed to write to log file: "
                          << ex.what() << "\n";
            }
        }
    }
}

// ============================================================================
// Configuration
// ============================================================================

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::set_output_file(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    if (file_path.empty()) {
        output_file_.clear();
        return;
    }
    output_file_ = std::filesystem::path(file_path);
    // Create parent directories if needed
    if (output_file_.has_parent_path()) {
        std::filesystem::create_directories(output_file_.parent_path());
    }
    file_stream_.open(output_file_, std::ios::app);
    if (!file_stream_.is_open()) {
        // Fail-safe: warn to stderr, don't throw
        std::cerr << "[ERROR] [Logger] Cannot open log file: " << file_path << "\n";
    }
}

void Logger::set_json_format(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    json_format_ = enabled;
}

// ============================================================================
// Export
// ============================================================================

std::string Logger::export_logs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string out = "[\n";
    for (size_t i = 0; i < entries_.size(); ++i) {
        out += "  " + entry_to_json(entries_[i]);
        if (i + 1 < entries_.size()) out += ",";
        out += "\n";
    }
    out += "]";
    return out;
}

std::string Logger::export_logs(LogLevel min_level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string out = "[\n";
    bool first = true;
    for (const auto& e : entries_) {
        if (e.level >= min_level) {
            if (!first) out += ",\n";
            out += "  " + entry_to_json(e);
            first = false;
        }
    }
    if (!first) out += "\n";
    out += "]";
    return out;
}

void Logger::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

// ============================================================================
// Helpers
// ============================================================================

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

std::string Logger::format_timestamp() {
    auto now     = std::chrono::system_clock::now();
    auto time_t  = std::chrono::system_clock::to_time_t(now);
    auto ms      = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch()) % 1000;

    struct tm tm_buf {};
#ifdef _MSC_VER
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::format_entry_text(const LogEntry& e) const {
    std::ostringstream oss;
    oss << "[" << e.timestamp << "]"
        << " [" << level_to_string(e.level) << "]"
        << " [" << e.component << "]";
    if (!e.tool_name.empty())  oss << " tool=" << e.tool_name;
    if (!e.session_id.empty()) oss << " session=" << e.session_id;
    oss << " " << e.message;
    if (!e.details.empty())    oss << " | " << e.details;
    return oss.str();
}

// Escape a string for JSON (minimal but robust)
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string Logger::entry_to_json(const LogEntry& e) const {
    std::ostringstream oss;
    oss << "{"
        << "\"timestamp\":\"" << json_escape(e.timestamp) << "\","
        << "\"level\":\""     << level_to_string(e.level) << "\","
        << "\"component\":\""  << json_escape(e.component) << "\","
        << "\"tool_name\":\""  << json_escape(e.tool_name) << "\","
        << "\"session_id\":\""<< json_escape(e.session_id) << "\","
        << "\"message\":\""   << json_escape(e.message)   << "\","
        << "\"details\":\""   << json_escape(e.details)   << "\""
        << "}";
    return oss.str();
}

} // namespace guardian
