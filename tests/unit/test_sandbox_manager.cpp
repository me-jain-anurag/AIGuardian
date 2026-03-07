// tests/unit/test_sandbox_manager.cpp
// Owner: Dev B
// Unit tests for MockRuntime and WasmExecutor (Task 2).
// No WasmEdge or .wasm files needed — purely in-memory.

#include <catch2/catch_test_macros.hpp>
#include "guardian/sandbox_manager.hpp"
#include <memory>

using namespace guardian;

// ============================================================================
// MockRuntime — basic interface (Task 2.2)
// ============================================================================

TEST_CASE("MockRuntime is loaded by default", "[sandbox][mock]") {
    MockRuntime mock;
    CHECK(mock.is_loaded());
}

TEST_CASE("MockRuntime returns default success when no result queued",
          "[sandbox][mock]") {
    MockRuntime mock;
    auto r = mock.execute("some_func", R"({"x":1})");
    CHECK(r.success);
    CHECK(r.output == "{}");
    CHECK_FALSE(r.error.has_value());
    CHECK_FALSE(r.violation.has_value());
}

TEST_CASE("MockRuntime records call metadata", "[sandbox][mock]") {
    MockRuntime mock;
    mock.execute("fn_a", R"({"key":"val"})");
    CHECK(mock.call_count() == 1);
    CHECK(mock.last_function_name() == "fn_a");
    CHECK(mock.last_params_json() == R"({"key":"val"})");

    mock.execute("fn_b", "{}");
    CHECK(mock.call_count() == 2);
    CHECK(mock.last_function_name() == "fn_b");
}

// ============================================================================
// MockRuntime::set_next_result / simulate_violation (Task 2.3)
// ============================================================================

TEST_CASE("MockRuntime returns queued results FIFO", "[sandbox][mock]") {
    MockRuntime mock;

    SandboxResult r1;
    r1.success = true;
    r1.output  = "first";

    SandboxResult r2;
    r2.success = true;
    r2.output  = "second";

    mock.set_next_result(r1);
    mock.set_next_result(r2);

    auto got1 = mock.execute("f", "{}");
    CHECK(got1.output == "first");

    auto got2 = mock.execute("f", "{}");
    CHECK(got2.output == "second");

    // Queue drained — falls back to default
    auto got3 = mock.execute("f", "{}");
    CHECK(got3.success);
    CHECK(got3.output == "{}");
}

TEST_CASE("MockRuntime::simulate_violation queues a violation result",
          "[sandbox][mock]") {
    MockRuntime mock;

    mock.simulate_violation(SandboxViolation::MEMORY_EXCEEDED,
                            "exceeded 128 MB");
    auto r = mock.execute("fn", "{}");

    CHECK_FALSE(r.success);
    REQUIRE(r.violation.has_value());
    CHECK(r.violation->type    == SandboxViolation::MEMORY_EXCEEDED);
    CHECK(r.violation->details == "exceeded 128 MB");
}

TEST_CASE("MockRuntime can simulate multiple different violations",
          "[sandbox][mock]") {
    MockRuntime mock;

    mock.simulate_violation(SandboxViolation::TIMEOUT, "5 s");
    mock.simulate_violation(SandboxViolation::FILE_ACCESS_DENIED, "/etc/passwd");
    mock.simulate_violation(SandboxViolation::NETWORK_DENIED, "socket blocked");

    auto v1 = mock.execute("a", "{}");
    CHECK(v1.violation->type == SandboxViolation::TIMEOUT);

    auto v2 = mock.execute("b", "{}");
    CHECK(v2.violation->type == SandboxViolation::FILE_ACCESS_DENIED);

    auto v3 = mock.execute("c", "{}");
    CHECK(v3.violation->type == SandboxViolation::NETWORK_DENIED);
}

TEST_CASE("MockRuntime returns error when not loaded", "[sandbox][mock]") {
    MockRuntime mock;
    mock.set_loaded(false);
    CHECK_FALSE(mock.is_loaded());

    auto r = mock.execute("fn", "{}");
    CHECK_FALSE(r.success);
    REQUIRE(r.error.has_value());
}

TEST_CASE("MockRuntime::reload restores loaded state", "[sandbox][mock]") {
    MockRuntime mock;
    mock.set_loaded(false);
    CHECK_FALSE(mock.is_loaded());

    mock.reload();
    CHECK(mock.is_loaded());
}

TEST_CASE("MockRuntime::reset clears history and queue", "[sandbox][mock]") {
    MockRuntime mock;
    mock.execute("fn", "p");
    mock.set_next_result(SandboxResult{});
    CHECK(mock.call_count() == 1);

    mock.reset();
    CHECK(mock.call_count() == 0);
    CHECK(mock.last_function_name().empty());
    CHECK(mock.last_params_json().empty());

    // Queue should be cleared — default result returned
    auto r = mock.execute("fn", "{}");
    CHECK(r.success);
    CHECK(r.output == "{}");
}

// ============================================================================
// WasmExecutor (Task 2.4)
// ============================================================================

TEST_CASE("WasmExecutor wraps MockRuntime and delegates execute",
          "[sandbox][executor]") {
    auto mock = std::make_unique<MockRuntime>();
    auto* mock_ptr = mock.get();  // keep raw ptr for inspection

    SandboxResult expected;
    expected.success = true;
    expected.output  = R"({"result":"ok"})";
    mock_ptr->set_next_result(expected);

    WasmExecutor exec(std::move(mock), "/path/to/tool.wasm");

    CHECK(exec.is_loaded());
    CHECK(exec.module_path() == "/path/to/tool.wasm");

    auto r = exec.execute("process", R"({"data":1})",
                          SandboxConfig::safe_defaults());
    CHECK(r.success);
    CHECK(r.output == R"({"result":"ok"})");
    CHECK(mock_ptr->call_count() == 1);
    CHECK(mock_ptr->last_function_name() == "process");
}

TEST_CASE("WasmExecutor returns error when runtime not loaded",
          "[sandbox][executor]") {
    auto mock = std::make_unique<MockRuntime>();
    mock->set_loaded(false);

    WasmExecutor exec(std::move(mock));
    auto r = exec.execute("fn", "{}", SandboxConfig::safe_defaults());
    CHECK_FALSE(r.success);
    REQUIRE(r.error.has_value());
}

TEST_CASE("WasmExecutor reload delegates to runtime", "[sandbox][executor]") {
    auto mock = std::make_unique<MockRuntime>();
    mock->set_loaded(false);
    auto* mock_ptr = mock.get();

    WasmExecutor exec(std::move(mock));
    CHECK_FALSE(exec.is_loaded());

    exec.reload();
    CHECK(exec.is_loaded());
    CHECK(mock_ptr->is_loaded());
}

TEST_CASE("WasmExecutor measures execution_time_ms", "[sandbox][executor]") {
    auto mock = std::make_unique<MockRuntime>();
    WasmExecutor exec(std::move(mock));

    auto r = exec.execute("fn", "{}", SandboxConfig::safe_defaults());
    CHECK(r.success);
    // execution_time_ms is set by WasmExecutor (uint32_t, always valid)
    CHECK(r.execution_time_ms <= 1000); // mock is near-instant
}

TEST_CASE("WasmExecutor propagates violations from runtime",
          "[sandbox][executor]") {
    auto mock = std::make_unique<MockRuntime>();
    mock->simulate_violation(SandboxViolation::MEMORY_EXCEEDED, "512 MB used");

    WasmExecutor exec(std::move(mock));
    auto r = exec.execute("fn", "{}", SandboxConfig::safe_defaults());

    CHECK_FALSE(r.success);
    REQUIRE(r.violation.has_value());
    CHECK(r.violation->type == SandboxViolation::MEMORY_EXCEEDED);
}

TEST_CASE("WasmExecutor rejects null runtime", "[sandbox][executor]") {
    CHECK_THROWS_AS(
        WasmExecutor(nullptr),
        std::invalid_argument);
}
