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

// ============================================================================
// Helper: MockRuntime factory for SandboxManager tests
// ============================================================================

/// Creates a RuntimeFactory that produces MockRuntime instances.
/// The optional callback lets tests configure each MockRuntime before use.
static RuntimeFactory make_mock_factory(
        std::function<void(MockRuntime&)> configure = nullptr) {
    return [configure](const std::string& /*path*/,
                       const SandboxConfig& /*cfg*/)
               -> std::unique_ptr<IWasmRuntime> {
        auto mock = std::make_unique<MockRuntime>();
        if (configure) { configure(*mock); }
        return mock;
    };
}

// ============================================================================
// SandboxManager — execute, violation, config (Task 5.1)
// ============================================================================

TEST_CASE("SandboxManager executes tool via MockRuntime",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory([](MockRuntime& m) {
        SandboxResult r;
        r.success = true;
        r.output  = R"({"data":"encrypted"})";
        m.set_next_result(r);
    }));

    mgr.load_module("encrypt", "encrypt.wasm");
    auto result = mgr.execute_tool("encrypt", R"({"key":"abc"})",
                                   SandboxConfig::safe_defaults());
    CHECK(result.success);
    CHECK(result.output == R"({"data":"encrypted"})");
}

TEST_CASE("SandboxManager propagates violations from runtime",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory([](MockRuntime& m) {
        m.simulate_violation(SandboxViolation::TIMEOUT, "exceeded 5s");
    }));

    mgr.load_module("slow_tool", "slow.wasm");
    auto r = mgr.execute_tool("slow_tool", "{}",
                              SandboxConfig::safe_defaults());
    CHECK_FALSE(r.success);
    REQUIRE(r.violation.has_value());
    CHECK(r.violation->type == SandboxViolation::TIMEOUT);
}

TEST_CASE("SandboxManager applies per-tool config",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());

    SandboxConfig custom;
    custom.memory_limit_mb = 256;
    custom.timeout_ms      = 10000;
    custom.network_access  = true;
    mgr.set_tool_config("net_tool", custom);

    auto cfg = mgr.get_config_for_tool("net_tool");
    CHECK(cfg.memory_limit_mb == 256);
    CHECK(cfg.timeout_ms      == 10000);
    CHECK(cfg.network_access  == true);
}

TEST_CASE("SandboxManager execute_tool with stored config (no explicit config)",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory([](MockRuntime& m) {
        SandboxResult r;
        r.success = true;
        r.output  = R"({"ok":true})";
        m.set_next_result(r);
    }));

    mgr.load_module("tool_a", "a.wasm");
    auto r = mgr.execute_tool("tool_a", "{}");  // uses default config
    CHECK(r.success);
}

// ============================================================================
// SandboxManager — module loading, caching, reuse (Task 5.2)
// ============================================================================

TEST_CASE("SandboxManager load_module makes has_module true",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());

    CHECK_FALSE(mgr.has_module("tool_x"));
    mgr.load_module("tool_x", "x.wasm");
    CHECK(mgr.has_module("tool_x"));
}

TEST_CASE("SandboxManager unload_module removes cached executor",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());

    mgr.load_module("tool_y", "y.wasm");
    CHECK(mgr.has_module("tool_y"));

    mgr.unload_module("tool_y");
    CHECK_FALSE(mgr.has_module("tool_y"));
}

TEST_CASE("SandboxManager load_module replaces existing executor",
          "[sandbox][manager]") {
    int load_count = 0;
    SandboxManager mgr("/tools", [&load_count](
            const std::string&, const SandboxConfig&) {
        ++load_count;
        return std::make_unique<MockRuntime>();
    });

    mgr.load_module("tool_z", "z.wasm");
    mgr.load_module("tool_z", "z_v2.wasm");  // reload replaces
    CHECK(load_count == 2);
    CHECK(mgr.has_module("tool_z"));
}

TEST_CASE("SandboxManager supports multiple concurrent modules",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());

    mgr.load_module("a", "a.wasm");
    mgr.load_module("b", "b.wasm");
    mgr.load_module("c", "c.wasm");

    CHECK(mgr.has_module("a"));
    CHECK(mgr.has_module("b"));
    CHECK(mgr.has_module("c"));

    mgr.unload_module("b");
    CHECK(mgr.has_module("a"));
    CHECK_FALSE(mgr.has_module("b"));
    CHECK(mgr.has_module("c"));
}

// ============================================================================
// SandboxManager — safe default configuration (Task 5.3)
// ============================================================================

TEST_CASE("SandboxManager returns safe defaults when no config set",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());

    auto cfg = mgr.get_config_for_tool("unknown_tool");
    CHECK(cfg.memory_limit_mb == 128);
    CHECK(cfg.timeout_ms      == 5000);
    CHECK(cfg.allowed_paths.empty());
    CHECK(cfg.network_access  == false);
}

TEST_CASE("SandboxManager set_default_config overrides safe defaults",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());

    SandboxConfig custom;
    custom.memory_limit_mb = 64;
    custom.timeout_ms      = 2000;
    mgr.set_default_config(custom);

    auto cfg = mgr.get_config_for_tool("any_tool");
    CHECK(cfg.memory_limit_mb == 64);
    CHECK(cfg.timeout_ms      == 2000);
}

TEST_CASE("SandboxManager per-tool config overrides default config",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());

    SandboxConfig default_cfg;
    default_cfg.memory_limit_mb = 64;
    mgr.set_default_config(default_cfg);

    SandboxConfig tool_cfg;
    tool_cfg.memory_limit_mb = 512;
    mgr.set_tool_config("heavy_tool", tool_cfg);

    // heavy_tool gets its per-tool config
    CHECK(mgr.get_config_for_tool("heavy_tool").memory_limit_mb == 512);
    // other tools get the default
    CHECK(mgr.get_config_for_tool("light_tool").memory_limit_mb == 64);
}

// ============================================================================
// SandboxManager — error handling (Task 5.4)
// ============================================================================

TEST_CASE("SandboxManager execute_tool returns error for missing module",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());

    auto r = mgr.execute_tool("nonexistent", "{}", SandboxConfig::safe_defaults());
    CHECK_FALSE(r.success);
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("not loaded") != std::string::npos);
}

TEST_CASE("SandboxManager load_module throws when no factory set",
          "[sandbox][manager]") {
    SandboxManager mgr;  // default constructor, no factory

    CHECK_THROWS_AS(
        mgr.load_module("tool", "tool.wasm"),
        std::runtime_error);
}

TEST_CASE("SandboxManager load_module throws when factory returns null",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", [](const std::string&, const SandboxConfig&) {
        return std::unique_ptr<IWasmRuntime>(nullptr);
    });

    CHECK_THROWS_AS(
        mgr.load_module("bad_tool", "bad.wasm"),
        std::runtime_error);
}

TEST_CASE("SandboxManager unload nonexistent module is safe",
          "[sandbox][manager]") {
    SandboxManager mgr("/tools", make_mock_factory());
    // Should not throw
    mgr.unload_module("never_loaded");
    CHECK_FALSE(mgr.has_module("never_loaded"));
}

TEST_CASE("SandboxManager wasm_tools_dir is accessible",
          "[sandbox][manager]") {
    SandboxManager mgr("/my/tools/dir", make_mock_factory());
    CHECK(mgr.wasm_tools_dir() == "/my/tools/dir");
}

TEST_CASE("SandboxManager set_runtime_factory enables deferred init",
          "[sandbox][manager]") {
    SandboxManager mgr;  // no factory yet

    CHECK_THROWS(mgr.load_module("t", "t.wasm"));

    mgr.set_runtime_factory(make_mock_factory());
    mgr.load_module("t", "t.wasm");  // now works
    CHECK(mgr.has_module("t"));
}

