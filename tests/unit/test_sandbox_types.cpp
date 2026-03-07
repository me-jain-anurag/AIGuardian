// tests/unit/test_sandbox_types.cpp
// Owner: Dev B
// Verifies SandboxConfig, SandboxResult, and SandboxViolation types
// match the spec and have correct safe defaults.

#include <catch2/catch_test_macros.hpp>
#include "guardian/types.hpp"

using namespace guardian;

// ---------------------------------------------------------------------------
// 1.1  SandboxConfig fields & 1.4 safe defaults
// ---------------------------------------------------------------------------

TEST_CASE("SandboxConfig default-constructed has safe defaults", "[sandbox][types]") {
    SandboxConfig cfg;

    CHECK(cfg.memory_limit_mb == 128);
    CHECK(cfg.timeout_ms      == 5000);
    CHECK(cfg.allowed_paths.empty());      // no file access
    CHECK(cfg.network_access   == false);   // no network
    CHECK(cfg.environment_vars.empty());
}

TEST_CASE("SandboxConfig::safe_defaults() returns safe values", "[sandbox][types]") {
    auto cfg = SandboxConfig::safe_defaults();

    CHECK(cfg.memory_limit_mb == 128);
    CHECK(cfg.timeout_ms      == 5000);
    CHECK(cfg.allowed_paths.empty());
    CHECK(cfg.network_access   == false);
    CHECK(cfg.environment_vars.empty());
}

TEST_CASE("SandboxConfig fields are assignable", "[sandbox][types]") {
    SandboxConfig cfg;
    cfg.memory_limit_mb = 256;
    cfg.timeout_ms      = 10000;
    cfg.allowed_paths   = {"/tmp", "/data"};
    cfg.network_access  = true;
    cfg.environment_vars["HOME"] = "/home/test";

    CHECK(cfg.memory_limit_mb == 256);
    CHECK(cfg.timeout_ms      == 10000);
    CHECK(cfg.allowed_paths.size() == 2);
    CHECK(cfg.network_access   == true);
    CHECK(cfg.environment_vars.at("HOME") == "/home/test");
}

// ---------------------------------------------------------------------------
// 1.2  SandboxResult fields
// ---------------------------------------------------------------------------

TEST_CASE("SandboxResult default-constructed is unsuccessful with no violation", "[sandbox][types]") {
    SandboxResult result;

    CHECK(result.success           == false);
    CHECK(result.output.empty());
    CHECK_FALSE(result.error.has_value());
    CHECK(result.memory_used_bytes == 0);
    CHECK(result.execution_time_ms == 0);
    CHECK_FALSE(result.violation.has_value());
}

TEST_CASE("SandboxResult can carry a violation", "[sandbox][types]") {
    SandboxResult result;
    result.success   = false;
    result.error     = "out of memory";
    result.violation = SandboxViolation{SandboxViolation::MEMORY_EXCEEDED,
                                        "exceeded 128 MB limit"};

    REQUIRE(result.violation.has_value());
    CHECK(result.violation->type    == SandboxViolation::MEMORY_EXCEEDED);
    CHECK(result.violation->details == "exceeded 128 MB limit");
}

// ---------------------------------------------------------------------------
// 1.3  SandboxViolation enum values
// ---------------------------------------------------------------------------

TEST_CASE("SandboxViolation::Type contains all required values", "[sandbox][types]") {
    // Simply instantiate each value — a compile-time check that they exist.
    SandboxViolation v1{SandboxViolation::MEMORY_EXCEEDED,    "mem"};
    SandboxViolation v2{SandboxViolation::TIMEOUT,            "time"};
    SandboxViolation v3{SandboxViolation::FILE_ACCESS_DENIED, "file"};
    SandboxViolation v4{SandboxViolation::NETWORK_DENIED,     "net"};

    CHECK(v1.type == SandboxViolation::MEMORY_EXCEEDED);
    CHECK(v2.type == SandboxViolation::TIMEOUT);
    CHECK(v3.type == SandboxViolation::FILE_ACCESS_DENIED);
    CHECK(v4.type == SandboxViolation::NETWORK_DENIED);
}
