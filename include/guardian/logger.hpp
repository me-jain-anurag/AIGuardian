// include/guardian/logger.hpp
// Owner: Dev D
// Logging infrastructure with level filtering and JSON/text output
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <filesystem>

namespace guardian {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

struct LogEntry {
    std::string timestamp;   // ISO-8601 formatted
    LogLevel    level;
    std::string component;
    std::string tool_name;
    std::string session_id;
    std::string message;
    std::string details;
};

class Logger {
public:
    // Construct with a minimum log level (messages below this are suppressed)
    explicit Logger(LogLevel min_level = LogLevel::INFO);
    ~Logger();

    // Non-copyable (owns file stream)
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Move is OK
    Logger(Logger&&) noexcept = default;
    Logger& operator=(Logger&&) noexcept = default;

    // ── Convenience logging methods ───────────────────────────────────────────
    void debug(const std::string& component, const std::string& message,
               const std::string& details = "");
    void info (const std::string& component, const std::string& message,
               const std::string& details = "");
    void warn (const std::string& component, const std::string& message,
               const std::string& details = "");
    void error(const std::string& component, const std::string& message,
               const std::string& details = "");

    // Extended log with tool/session context
    void log_with_context(LogLevel level,
                          const std::string& component,
                          const std::string& message,
                          const std::string& tool_name,
                          const std::string& session_id,
                          const std::string& details = "");

    // ── Configuration ─────────────────────────────────────────────────────────
    void set_level(LogLevel level);
    void set_output_file(const std::string& file_path);  // "" = disable file output
    void set_json_format(bool enabled);                   // false = plain text

    // ── Export ────────────────────────────────────────────────────────────────
    std::string export_logs() const;                      // all entries as JSON array
    std::string export_logs(LogLevel min_level) const;   // filtered by level

    // Clear stored entries (e.g., after export)
    void clear();

    // ── Singleton accessor (for backward compatibility with guardian.cpp) ─────
    // Returns a process-wide default Logger instance.
    static Logger& instance();

    // ── Helpers ───────────────────────────────────────────────────────────────
    static std::string level_to_string(LogLevel level);

private:
    void log_impl(LogLevel level,
                  const std::string& component,
                  const std::string& message,
                  const std::string& tool_name,
                  const std::string& session_id,
                  const std::string& details);

    static std::string format_timestamp();
    std::string format_entry_text(const LogEntry& e) const;
    std::string entry_to_json(const LogEntry& e) const;

    LogLevel              min_level_;
    bool                  json_format_ = false;
    std::filesystem::path output_file_;
    std::ofstream         file_stream_;

    mutable std::mutex    mutex_;
    std::vector<LogEntry> entries_;
};

} // namespace guardian
