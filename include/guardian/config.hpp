// include/guardian/config.hpp
// Owner: Dev A
// Configuration management with JSON loading and safe defaults
#pragma once

#include "types.hpp"
#include <string>
#include <cstdint>

namespace guardian {

struct CycleDetectionConfig {
    uint32_t default_threshold = 10;
    std::map<std::string, uint32_t> per_tool_thresholds;
};

struct PerformanceConfig {
    uint32_t cache_size = 1000;
    bool enable_string_interning = true;
};

struct LoggingConfig {
    std::string level = "INFO";
    std::string format = "text"; // "text" or "json"
    std::string output = "console"; // "console" or file path
};

struct PolicyConfig {
    std::string default_policy_path;
    std::string wasm_tools_dir;
};

struct Config {
    CycleDetectionConfig cycle_detection;
    SandboxConfig sandbox; // reuse from types.hpp
    PerformanceConfig performance;
    LoggingConfig logging;
    PolicyConfig policy;

    // Load from JSON file, apply safe defaults for missing/invalid values
    static Config load(const std::string& config_file_path);

    // Serialization
    std::string to_json() const;
    static Config from_json(const std::string& json_str);
};

} // namespace guardian
