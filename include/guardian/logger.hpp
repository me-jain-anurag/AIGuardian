// include/guardian/logger.hpp
// Owner: Dev D
// Logging infrastructure with level filtering and JSON/text output
#pragma once

#include <string>
#include <vector>

namespace guardian {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

struct LogEntry {
    std::string timestamp;
    LogLevel level;
    std::string component;
    std::string tool_name;
    std::string session_id;
    std::string message;
    std::string details;
};

class Logger {
public:
    static Logger& instance();

    explicit Logger(LogLevel min_level = LogLevel::INFO);
    ~Logger() = default;

    // Logging methods
    void log(LogLevel level, const std::string& component, const std::string& message);
    void debug(const std::string& component, const std::string& message,
               const std::string& details = "");
    void info(const std::string& component, const std::string& message,
              const std::string& details = "");
    void warn(const std::string& component, const std::string& message,
              const std::string& details = "");
    void error(const std::string& component, const std::string& message,
               const std::string& details = "");

    // Configuration
    void set_level(LogLevel level);
    void set_output_file(const std::string& file_path);
    void set_json_format(bool enabled);

    // Export
    std::string export_logs() const;
    std::string export_logs(LogLevel min_level) const;

    void set_format(const std::string& format);
    static std::string level_to_string(LogLevel level);

private:
    LogLevel min_level_ = LogLevel::INFO;
    std::string format_ = "text";
};

} // namespace guardian
