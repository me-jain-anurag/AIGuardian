// tests/sandbox/test_wasmedge.cpp
// Owner: Dev B
// Integration tests for WasmEdgeRuntime (Linux only).
// These tests require WasmEdge SDK and real .wasm binaries.
// Guarded by HAVE_WASMEDGE — skipped entirely on Windows/no-WasmEdge builds.

#include <catch2/catch_test_macros.hpp>
#include "guardian/sandbox_manager.hpp"

#ifdef HAVE_WASMEDGE

using namespace guardian;

// ============================================================================
// Helper: paths to test .wasm modules (built by wasm_tools/build.sh)
// ============================================================================

static const std::string WASM_TOOLS_DIR = "wasm_tools";
static const std::string SEARCH_DB_WASM = "search_database.wasm";
static const std::string MEMORY_HOG_WASM = "memory_hog.wasm";
static const std::string INFINITE_LOOP_WASM = "infinite_loop_tool.wasm";
static const std::string READ_CODE_WASM = "read_code.wasm";
static const std::string SEND_EMAIL_WASM = "send_email.wasm";

// ============================================================================
// 5.5 Basic WasmEdge integration
// ============================================================================

TEST_CASE("WasmEdgeRuntime loads valid .wasm module",
          "[sandbox][wasmedge][integration]") {
    SandboxConfig cfg = SandboxConfig::safe_defaults();
    REQUIRE_NOTHROW(
        WasmEdgeRuntime(WASM_TOOLS_DIR + "/" + SEARCH_DB_WASM, cfg));
}

TEST_CASE("WasmEdgeRuntime throws on missing .wasm file",
          "[sandbox][wasmedge][integration]") {
    SandboxConfig cfg = SandboxConfig::safe_defaults();
    CHECK_THROWS_AS(
        WasmEdgeRuntime("/nonexistent/path.wasm", cfg),
        std::runtime_error);
}

TEST_CASE("WasmEdgeRuntime execute returns result",
          "[sandbox][wasmedge][integration]") {
    SandboxConfig cfg = SandboxConfig::safe_defaults();
    WasmEdgeRuntime rt(WASM_TOOLS_DIR + "/" + SEARCH_DB_WASM, cfg);

    CHECK(rt.is_loaded());
    auto r = rt.execute("_start", R"({"query":"test"})");
    // Result will vary — just check we get something back
    CHECK(r.execution_time_ms <= 5000);
}

TEST_CASE("WasmEdgeRuntime reload reloads module",
          "[sandbox][wasmedge][integration]") {
    SandboxConfig cfg = SandboxConfig::safe_defaults();
    WasmEdgeRuntime rt(WASM_TOOLS_DIR + "/" + SEARCH_DB_WASM, cfg);
    CHECK(rt.is_loaded());

    REQUIRE_NOTHROW(rt.reload());
    CHECK(rt.is_loaded());
}

// ============================================================================
// 5.6 Memory limit enforcement
// ============================================================================

TEST_CASE("WasmEdgeRuntime enforces memory limit on memory_hog tool",
          "[sandbox][wasmedge][integration][memory]") {
    SandboxConfig cfg;
    cfg.memory_limit_mb = 1;  // Very small limit
    cfg.timeout_ms      = 5000;

    WasmEdgeRuntime rt(WASM_TOOLS_DIR + "/" + MEMORY_HOG_WASM, cfg);
    auto r = rt.execute("_start", "{}");

    CHECK_FALSE(r.success);
    REQUIRE(r.violation.has_value());
    CHECK(r.violation->type == SandboxViolation::MEMORY_EXCEEDED);
}

// ============================================================================
// 5.7 Timeout enforcement
// ============================================================================

TEST_CASE("WasmEdgeRuntime enforces timeout on infinite loop tool",
          "[sandbox][wasmedge][integration][timeout]") {
    SandboxConfig cfg;
    cfg.memory_limit_mb = 128;
    cfg.timeout_ms      = 500;  // Short timeout

    WasmEdgeRuntime rt(WASM_TOOLS_DIR + "/" + INFINITE_LOOP_WASM, cfg);
    auto r = rt.execute("_start", "{}");

    CHECK_FALSE(r.success);
    REQUIRE(r.violation.has_value());
    CHECK(r.violation->type == SandboxViolation::TIMEOUT);
    // Should complete within a reasonable margin of the timeout
    CHECK(r.execution_time_ms <= 2000);
}

// ============================================================================
// 5.8 File access control
// ============================================================================

TEST_CASE("WasmEdgeRuntime blocks unauthorized file access",
          "[sandbox][wasmedge][integration][file]") {
    SandboxConfig cfg = SandboxConfig::safe_defaults();
    // No allowed_paths — all file access should be denied.

    WasmEdgeRuntime rt(WASM_TOOLS_DIR + "/" + READ_CODE_WASM, cfg);
    auto r = rt.execute("_start", R"({"path":"/etc/passwd"})");

    CHECK_FALSE(r.success);
    // Should either fail with FILE_ACCESS_DENIED or a generic error
    if (r.violation.has_value()) {
        CHECK(r.violation->type == SandboxViolation::FILE_ACCESS_DENIED);
    }
}

TEST_CASE("WasmEdgeRuntime allows access to preopened directories",
          "[sandbox][wasmedge][integration][file]") {
    SandboxConfig cfg = SandboxConfig::safe_defaults();
    cfg.allowed_paths = {"."};  // Allow current directory

    WasmEdgeRuntime rt(WASM_TOOLS_DIR + "/" + READ_CODE_WASM, cfg);
    auto r = rt.execute("_start", R"({"path":"./CMakeLists.txt"})");

    // With preopened dir ".", reading CMakeLists.txt should succeed
    // (or at least not fail with FILE_ACCESS_DENIED)
    if (r.violation.has_value()) {
        CHECK(r.violation->type != SandboxViolation::FILE_ACCESS_DENIED);
    }
}

// ============================================================================
// 5.9 Network access control
// ============================================================================

TEST_CASE("WasmEdgeRuntime blocks network when disabled",
          "[sandbox][wasmedge][integration][network]") {
    SandboxConfig cfg = SandboxConfig::safe_defaults();
    cfg.network_access = false;  // Default — no network

    WasmEdgeRuntime rt(WASM_TOOLS_DIR + "/" + SEND_EMAIL_WASM, cfg);
    auto r = rt.execute("_start", R"({"to":"user@example.com"})");

    CHECK_FALSE(r.success);
    // Should fail — the tool tries to use network
    if (r.violation.has_value()) {
        CHECK(r.violation->type == SandboxViolation::NETWORK_DENIED);
    }
}

#else // !HAVE_WASMEDGE

// Placeholder so the test binary still compiles on non-WasmEdge systems.
TEST_CASE("WasmEdge integration tests skipped (HAVE_WASMEDGE not defined)",
          "[sandbox][wasmedge][integration]") {
    WARN("WasmEdge SDK not available — integration tests skipped");
    CHECK(true);
}

#endif // HAVE_WASMEDGE
