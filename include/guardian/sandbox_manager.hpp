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
// WasmEdgeRuntime — real WasmEdge integration (Task 3, Linux only)
// ============================================================================

#ifdef HAVE_WASMEDGE

/// Production runtime wrapping the WasmEdge C SDK.
/// Enforces memory limits, timeouts, file access control, and network control.
///
/// Guarded by HAVE_WASMEDGE — Windows builds skip this entirely and use
/// MockRuntime instead.
///
/// Usage:
///   SandboxConfig cfg = SandboxConfig::safe_defaults();
///   cfg.allowed_paths = {"/data"};
///   auto rt = std::make_unique<WasmEdgeRuntime>("/path/to/tool.wasm", cfg);
///   auto r = rt->execute("process", R"({"key":"val"})");
class WasmEdgeRuntime : public IWasmRuntime {
public:
    /// Load a Wasm module from disk and configure the VM with the given
    /// sandbox constraints.
    /// @param wasm_path  Path to the .wasm binary.
    /// @param config     Sandbox constraints (memory, timeout, paths, network).
    /// @throws std::runtime_error on load failure.
    WasmEdgeRuntime(const std::string& wasm_path, const SandboxConfig& config);
    ~WasmEdgeRuntime() override;

    // Non-copyable
    WasmEdgeRuntime(const WasmEdgeRuntime&) = delete;
    WasmEdgeRuntime& operator=(const WasmEdgeRuntime&) = delete;

    // ---- IWasmRuntime interface -------------------------------------------
    SandboxResult execute(const std::string& function_name,
                          const std::string& params_json) override;
    bool is_loaded() const override;
    void reload() override;

private:
    /// Apply SandboxConfig to WasmEdge configure context.
    void apply_config(const SandboxConfig& config);

    /// Load the module from wasm_path_ into the VM.
    void load_module();

    /// Extract output from Wasm function return values as JSON string.
    std::string extract_result_json() const;

    struct Impl;                       // PIMPL for WasmEdge SDK types
    std::unique_ptr<Impl> impl_;
    std::string wasm_path_;
    SandboxConfig config_;
    bool loaded_ = false;
};

#endif // HAVE_WASMEDGE

// ============================================================================
// SandboxManager — full implementation (Task 4)
// ============================================================================

/// Manages a pool of WasmExecutor instances keyed by tool name.
/// Thread-safe: uses std::shared_mutex for concurrent reads / exclusive writes.
///
/// Usage:
///   // Create with a factory (MockRuntime for tests, WasmEdgeRuntime for prod)
///   auto factory = [](const std::string& path, const SandboxConfig& cfg) {
///       return std::make_unique<MockRuntime>();
///   };
///   SandboxManager mgr("/path/to/wasm_tools", factory);
///   mgr.load_module("encrypt", "encrypt.wasm");
///   auto result = mgr.execute_tool("encrypt", R"({"data":"..."})",
///                                   SandboxConfig::safe_defaults());
class SandboxManager {
public:
    /// Construct with a wasm_tools directory and a RuntimeFactory.
    /// @param wasm_tools_dir  Base directory for .wasm files.
    /// @param factory         Creates IWasmRuntime instances from a path+config.
    SandboxManager(const std::string& wasm_tools_dir,
                   RuntimeFactory factory);

    /// Construct with defaults (no factory — load_module will fail until set).
    SandboxManager() = default;
    ~SandboxManager() = default;

    // Non-copyable
    SandboxManager(const SandboxManager&) = delete;
    SandboxManager& operator=(const SandboxManager&) = delete;

    // ---- Module management (Task 4.1, 4.2, 4.4) ---------------------------

    /// Load a Wasm module and cache its executor.
    /// @param tool_name  Logical name for the tool (e.g. "encrypt").
    /// @param wasm_path  Path to the .wasm file (relative to wasm_tools_dir
    ///                   or absolute).
    void load_module(const std::string& tool_name, const std::string& wasm_path);

    /// Unload a cached executor.
    void unload_module(const std::string& tool_name);

    /// Check whether a tool is loaded.
    bool has_module(const std::string& tool_name) const;

    // ---- Execution (Task 4.3) ---------------------------------------------

    /// Execute a tool: lookup module → apply config → run.
    /// Uses the per-tool config if set, otherwise the default config,
    /// unless an explicit config is provided.
    SandboxResult execute_tool(const std::string& tool_name,
                                const std::string& params_json,
                                const SandboxConfig& config);

    /// Execute a tool using its stored config (per-tool or default).
    SandboxResult execute_tool(const std::string& tool_name,
                                const std::string& params_json);

    // ---- Configuration (Task 4.6, 4.7, 4.8) ------------------------------

    /// Set the default sandbox config used when no per-tool config exists.
    void set_default_config(const SandboxConfig& config);

    /// Set a per-tool sandbox config.
    void set_tool_config(const std::string& tool_name,
                         const SandboxConfig& config);

    /// Get the effective config for a tool (per-tool → default → safe).
    SandboxConfig get_config_for_tool(const std::string& tool_name) const;

    /// Get the wasm_tools directory.
    const std::string& wasm_tools_dir() const;

    /// Set the RuntimeFactory (for deferred initialization).
    void set_runtime_factory(RuntimeFactory factory);

private:
    std::string wasm_tools_dir_;
    RuntimeFactory factory_;
    SandboxConfig default_config_ = SandboxConfig::safe_defaults();

    // Thread-safe executor pool (Task 4.5)
    mutable std::shared_mutex executors_mutex_;
    std::map<std::string, std::unique_ptr<WasmExecutor>> executors_;

    // Per-tool config storage (Task 4.6)
    mutable std::shared_mutex configs_mutex_;
    std::map<std::string, SandboxConfig> tool_configs_;
};

} // namespace guardian

