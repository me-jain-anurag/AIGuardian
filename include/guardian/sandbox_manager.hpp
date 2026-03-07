// include/guardian/sandbox_manager.hpp
// Owner: Dev B
// WasmEdge sandbox manager with runtime abstraction
//
// Cross-platform: MockRuntime and WasmExecutor compile on both Linux and
// Windows (no platform-specific headers).  WasmEdgeRuntime (Task 3) is
// guarded by #ifdef HAVE_WASMEDGE.
#pragma once

#include "types.hpp"
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <filesystem>
#include <queue>

namespace guardian {

// ============================================================================
// IWasmRuntime — pure interface (Task 2.1)
// ============================================================================

/// Abstract runtime that can execute a named Wasm function with JSON params.
/// Dev B implements WasmEdgeRuntime (real) and MockRuntime (test-only).
class IWasmRuntime {
public:
    virtual ~IWasmRuntime() = default;

    /// Execute a function inside the loaded Wasm module.
    /// @param function_name  The exported function to call.
    /// @param params_json    JSON-encoded parameter object.
    /// @return SandboxResult with success/output/error/violation info.
    virtual SandboxResult execute(const std::string& function_name,
                                   const std::string& params_json) = 0;

    /// Returns true when a valid Wasm module is loaded and ready.
    virtual bool is_loaded() const = 0;

    /// Reload the Wasm module from its original path (hot-swap).
    virtual void reload() = 0;
};

// Factory type used by SandboxManager to create runtimes.
using RuntimeFactory = std::function<
    std::unique_ptr<IWasmRuntime>(const std::string& wasm_path,
                                   const SandboxConfig& config)>;

// ============================================================================
// MockRuntime — test double (Tasks 2.2, 2.3)
// ============================================================================

/// A deterministic, in-memory runtime for unit testing.
/// No WasmEdge, no .wasm files — works on every platform.
///
/// Usage:
///   auto mock = std::make_unique<MockRuntime>();
///   mock->set_next_result(SandboxResult{true, R"({"ok":1})", ...});
///   auto r = mock->execute("fn", "{}");   // returns the queued result
///
///   mock->simulate_violation(SandboxViolation::TIMEOUT, "hit 5 s limit");
///   auto r2 = mock->execute("fn", "{}");  // returns a violation result
class MockRuntime : public IWasmRuntime {
public:
    MockRuntime();
    ~MockRuntime() override = default;

    // ---- IWasmRuntime interface -------------------------------------------
    SandboxResult execute(const std::string& function_name,
                          const std::string& params_json) override;
    bool is_loaded() const override;
    void reload() override;

    // ---- Test control (Task 2.3) ------------------------------------------

    /// Queue a result that will be returned by the next execute() call.
    /// Multiple calls queue results FIFO; when the queue is empty, execute()
    /// falls back to a default "success" result.
    void set_next_result(const SandboxResult& result);

    /// Convenience: queue a failure result carrying a SandboxViolation.
    void simulate_violation(SandboxViolation::Type type,
                            const std::string& details = "");

    /// Mark the runtime as unloaded (is_loaded() returns false).
    void set_loaded(bool loaded);

    // ---- Inspection helpers -----------------------------------------------

    /// Number of times execute() has been called since construction / reset.
    size_t call_count() const;

    /// The function_name passed to the most recent execute() call.
    const std::string& last_function_name() const;

    /// The params_json passed to the most recent execute() call.
    const std::string& last_params_json() const;

    /// Reset call history and result queue.
    void reset();

private:
    bool loaded_ = true;
    size_t call_count_ = 0;
    std::string last_function_;
    std::string last_params_;
    std::queue<SandboxResult> result_queue_;
};

// ============================================================================
// WasmExecutor — thin wrapper around IWasmRuntime (Task 2.4)
// ============================================================================

/// Owns an IWasmRuntime instance, applies a SandboxConfig before each
/// execution, and records timing.  This is the unit that SandboxManager
/// caches per tool.
class WasmExecutor {
public:
    /// Construct from any IWasmRuntime (real or mock).
    explicit WasmExecutor(std::unique_ptr<IWasmRuntime> runtime,
                          const std::string& module_path = "");
    ~WasmExecutor() = default;

    // Non-copyable, movable
    WasmExecutor(const WasmExecutor&) = delete;
    WasmExecutor& operator=(const WasmExecutor&) = delete;
    WasmExecutor(WasmExecutor&&) = default;
    WasmExecutor& operator=(WasmExecutor&&) = default;

    /// Execute a function with the given config constraints.
    SandboxResult execute(const std::string& function_name,
                          const std::string& params_json,
                          const SandboxConfig& config);

    /// Is the underlying runtime loaded?
    bool is_loaded() const;

    /// Reload the underlying Wasm module.
    void reload();

    /// Path of the .wasm file this executor was created from.
    const std::string& module_path() const;

private:
    std::unique_ptr<IWasmRuntime> runtime_;
    std::string module_path_;
};

// ============================================================================
// SandboxManager (Task 4 — stub kept for forward-compat)
// ============================================================================

class SandboxManager {
public:
    SandboxManager() = default;
    ~SandboxManager() = default;

    // Module management
    void load_module(const std::string& tool_name, const std::string& wasm_path);
    void unload_module(const std::string& tool_name);
    bool has_module(const std::string& tool_name) const;

    // Execution
    SandboxResult execute_tool(const std::string& tool_name,
                                const std::string& params_json,
                                const SandboxConfig& config);

    // Configuration
    void set_default_config(const SandboxConfig& config);
    SandboxConfig get_config_for_tool(const std::string& tool_name) const;
};

} // namespace guardian
