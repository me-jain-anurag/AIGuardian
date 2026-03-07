// tests/unit/test_config.cpp
// Owner: Dev A
// Unit tests for Config loading, defaults, and serialization
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "guardian/config.hpp"

using namespace guardian;

// ============================================================================
// Default Config Tests
// ============================================================================

TEST_CASE("Default config from empty JSON", "[config]") {
    auto cfg = Config::from_json("{}");

    REQUIRE(cfg.cycle_detection.default_threshold == 10);
    REQUIRE(cfg.sandbox.memory_limit_mb == 128);
    REQUIRE(cfg.sandbox.timeout_ms == 5000);
    REQUIRE(cfg.sandbox.network_access == false);
    REQUIRE(cfg.performance.cache_size == 1000);
    REQUIRE(cfg.logging.level == "INFO");
    REQUIRE(cfg.logging.format == "text");
    REQUIRE(cfg.logging.output == "console");
}

TEST_CASE("Invalid JSON returns defaults", "[config][error]") {
    auto cfg = Config::from_json("{not valid json}");

    REQUIRE(cfg.cycle_detection.default_threshold == 10);
    REQUIRE(cfg.sandbox.memory_limit_mb == 128);
}

// ============================================================================
// Value Override Tests
// ============================================================================

TEST_CASE("Config values override defaults", "[config]") {
    auto cfg = Config::from_json(R"({
        "cycle_detection": { "default_threshold": 20 },
        "sandbox": { "memory_limit_mb": 256, "timeout_ms": 10000 },
        "logging": { "level": "DEBUG", "format": "json" }
    })");

    REQUIRE(cfg.cycle_detection.default_threshold == 20);
    REQUIRE(cfg.sandbox.memory_limit_mb == 256);
    REQUIRE(cfg.sandbox.timeout_ms == 10000);
    REQUIRE(cfg.logging.level == "DEBUG");
    REQUIRE(cfg.logging.format == "json");

    // Non-overridden defaults should remain
    REQUIRE(cfg.sandbox.network_access == false);
    REQUIRE(cfg.performance.cache_size == 1000);
}

TEST_CASE("Per-tool cycle thresholds", "[config]") {
    auto cfg = Config::from_json(R"({
        "cycle_detection": {
            "default_threshold": 10,
            "per_tool_thresholds": { "search_database": 5, "read_code": 3 }
        }
    })");

    REQUIRE(cfg.cycle_detection.per_tool_thresholds.size() == 2);
    REQUIRE(cfg.cycle_detection.per_tool_thresholds.at("search_database") == 5);
    REQUIRE(cfg.cycle_detection.per_tool_thresholds.at("read_code") == 3);
}

// ============================================================================
// Validation Tests (out-of-range values)
// ============================================================================

TEST_CASE("Memory limit out of range uses default", "[config][validation]") {
    auto cfg = Config::from_json(R"({ "sandbox": { "memory_limit_mb": 99999 } })");
    REQUIRE(cfg.sandbox.memory_limit_mb == 128); // default
}

TEST_CASE("Timeout out of range uses default", "[config][validation]") {
    auto cfg = Config::from_json(R"({ "sandbox": { "timeout_ms": 0 } })");
    REQUIRE(cfg.sandbox.timeout_ms == 5000); // default
}

TEST_CASE("Unknown log level uses default", "[config][validation]") {
    auto cfg = Config::from_json(R"({ "logging": { "level": "VERBOSE" } })");
    REQUIRE(cfg.logging.level == "INFO"); // default
}

// ============================================================================
// Round-Trip Tests
// ============================================================================

TEST_CASE("Config JSON round-trip", "[config][serialization]") {
    Config original;
    original.cycle_detection.default_threshold = 15;
    original.sandbox.memory_limit_mb = 256;
    original.sandbox.timeout_ms = 3000;
    original.sandbox.network_access = true;
    original.logging.level = "DEBUG";
    original.logging.format = "json";
    original.policy.default_policy_path = "policies/test.json";

    std::string json_str = original.to_json();
    auto restored = Config::from_json(json_str);

    REQUIRE(restored.cycle_detection.default_threshold == 15);
    REQUIRE(restored.sandbox.memory_limit_mb == 256);
    REQUIRE(restored.sandbox.timeout_ms == 3000);
    REQUIRE(restored.sandbox.network_access == true);
    REQUIRE(restored.logging.level == "DEBUG");
    REQUIRE(restored.logging.format == "json");
    REQUIRE(restored.policy.default_policy_path == "policies/test.json");
}

// ============================================================================
// File Loading Tests
// ============================================================================

TEST_CASE("Load from nonexistent file returns defaults", "[config][file]") {
    auto cfg = Config::load("nonexistent_config_file.json");
    REQUIRE(cfg.cycle_detection.default_threshold == 10);
    REQUIRE(cfg.sandbox.memory_limit_mb == 128);
}
