// src/config.cpp
// Owner: Dev A
// Configuration management — load/save JSON with safe defaults
#include "guardian/config.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace guardian {

// ============================================================================
// Safe Default Config
// ============================================================================

static Config make_default_config() {
    Config cfg;
    // CycleDetection defaults
    cfg.cycle_detection.default_threshold = 10;
    cfg.cycle_detection.per_tool_thresholds = {};

    // Sandbox defaults (safe — restrictive)
    cfg.sandbox.memory_limit_mb = 128;
    cfg.sandbox.timeout_ms = 5000;
    cfg.sandbox.allowed_paths = {};
    cfg.sandbox.network_access = false;
    cfg.sandbox.environment_vars = {};

    // Performance defaults
    cfg.performance.cache_size = 1000;
    cfg.performance.enable_string_interning = true;

    // Logging defaults
    cfg.logging.level = "INFO";
    cfg.logging.format = "text";
    cfg.logging.output = "console";

    // Policy defaults
    cfg.policy.default_policy_path = "";
    cfg.policy.wasm_tools_dir = "";

    return cfg;
}

// ============================================================================
// JSON Serialization
// ============================================================================

std::string Config::to_json() const {
    json j;

    // Cycle detection
    j["cycle_detection"] = {
        {"default_threshold", cycle_detection.default_threshold},
        {"per_tool_thresholds", cycle_detection.per_tool_thresholds}
    };

    // Sandbox
    j["sandbox"] = {
        {"memory_limit_mb", sandbox.memory_limit_mb},
        {"timeout_ms", sandbox.timeout_ms},
        {"allowed_paths", sandbox.allowed_paths},
        {"network_access", sandbox.network_access},
        {"environment_vars", sandbox.environment_vars}
    };

    // Performance
    j["performance"] = {
        {"cache_size", performance.cache_size},
        {"enable_string_interning", performance.enable_string_interning}
    };

    // Logging
    j["logging"] = {
        {"level", logging.level},
        {"format", logging.format},
        {"output", logging.output}
    };

    // Policy
    j["policy"] = {
        {"default_policy_path", policy.default_policy_path},
        {"wasm_tools_dir", policy.wasm_tools_dir}
    };

    return j.dump(2);
}

Config Config::from_json(const std::string& json_str) {
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        std::cerr << "[WARN] Config::from_json: invalid JSON, using defaults — "
                  << e.what() << std::endl;
        return make_default_config();
    }

    Config cfg = make_default_config(); // start with safe defaults

    // Cycle detection
    if (j.contains("cycle_detection")) {
        auto& cd = j["cycle_detection"];
        if (cd.contains("default_threshold") && cd["default_threshold"].is_number_unsigned()) {
            cfg.cycle_detection.default_threshold = cd["default_threshold"].get<uint32_t>();
        }
        if (cd.contains("per_tool_thresholds") && cd["per_tool_thresholds"].is_object()) {
            cfg.cycle_detection.per_tool_thresholds =
                cd["per_tool_thresholds"].get<std::map<std::string, uint32_t>>();
        }
    }

    // Sandbox
    if (j.contains("sandbox")) {
        auto& sb = j["sandbox"];
        if (sb.contains("memory_limit_mb") && sb["memory_limit_mb"].is_number()) {
            auto val = sb["memory_limit_mb"].get<uint64_t>();
            if (val > 0 && val <= 4096) {
                cfg.sandbox.memory_limit_mb = val;
            } else {
                std::cerr << "[WARN] Config: memory_limit_mb " << val
                          << " out of range (1-4096), using default 128" << std::endl;
            }
        }
        if (sb.contains("timeout_ms") && sb["timeout_ms"].is_number()) {
            auto val = sb["timeout_ms"].get<uint32_t>();
            if (val >= 100 && val <= 60000) {
                cfg.sandbox.timeout_ms = val;
            } else {
                std::cerr << "[WARN] Config: timeout_ms " << val
                          << " out of range (100-60000), using default 5000" << std::endl;
            }
        }
        if (sb.contains("allowed_paths") && sb["allowed_paths"].is_array()) {
            cfg.sandbox.allowed_paths = sb["allowed_paths"].get<std::vector<std::string>>();
        }
        if (sb.contains("network_access") && sb["network_access"].is_boolean()) {
            cfg.sandbox.network_access = sb["network_access"].get<bool>();
        }
        if (sb.contains("environment_vars") && sb["environment_vars"].is_object()) {
            cfg.sandbox.environment_vars =
                sb["environment_vars"].get<std::map<std::string, std::string>>();
        }
    }

    // Performance
    if (j.contains("performance")) {
        auto& perf = j["performance"];
        if (perf.contains("cache_size") && perf["cache_size"].is_number_unsigned()) {
            cfg.performance.cache_size = perf["cache_size"].get<uint32_t>();
        }
        if (perf.contains("enable_string_interning") && perf["enable_string_interning"].is_boolean()) {
            cfg.performance.enable_string_interning = perf["enable_string_interning"].get<bool>();
        }
    }

    // Logging
    if (j.contains("logging")) {
        auto& log = j["logging"];
        if (log.contains("level") && log["level"].is_string()) {
            std::string lvl = log["level"].get<std::string>();
            if (lvl == "DEBUG" || lvl == "INFO" || lvl == "WARN" || lvl == "ERROR") {
                cfg.logging.level = lvl;
            } else {
                std::cerr << "[WARN] Config: unknown log level '" << lvl
                          << "', using default INFO" << std::endl;
            }
        }
        if (log.contains("format") && log["format"].is_string()) {
            std::string fmt = log["format"].get<std::string>();
            if (fmt == "text" || fmt == "json") {
                cfg.logging.format = fmt;
            }
        }
        if (log.contains("output") && log["output"].is_string()) {
            cfg.logging.output = log["output"].get<std::string>();
        }
    }

    // Policy
    if (j.contains("policy")) {
        auto& pol = j["policy"];
        if (pol.contains("default_policy_path") && pol["default_policy_path"].is_string()) {
            cfg.policy.default_policy_path = pol["default_policy_path"].get<std::string>();
        }
        if (pol.contains("wasm_tools_dir") && pol["wasm_tools_dir"].is_string()) {
            cfg.policy.wasm_tools_dir = pol["wasm_tools_dir"].get<std::string>();
        }
    }

    return cfg;
}

// ============================================================================
// File Loading
// ============================================================================

Config Config::load(const std::string& config_file_path) {
    fs::path path(config_file_path);

    if (!fs::exists(path)) {
        std::cerr << "[WARN] Config::load: file '" << config_file_path
                  << "' not found, using defaults" << std::endl;
        return make_default_config();
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[WARN] Config::load: could not open '" << config_file_path
                  << "', using defaults" << std::endl;
        return make_default_config();
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return Config::from_json(content);
}

} // namespace guardian
