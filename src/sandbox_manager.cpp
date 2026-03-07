// src/sandbox_manager.cpp
// Owner: Dev B
// Implements MockRuntime, WasmExecutor, and SandboxManager stubs.
//
// Cross-platform: only standard C++17 — compiles on both MSVC and GCC/Clang.

#include "guardian/sandbox_manager.hpp"
#include <chrono>
#include <stdexcept>

namespace guardian {

// ============================================================================
// MockRuntime
// ============================================================================

MockRuntime::MockRuntime() = default;

SandboxResult MockRuntime::execute(const std::string& function_name,
                                   const std::string& params_json) {
    ++call_count_;
    last_function_ = function_name;
    last_params_   = params_json;

    if (!loaded_) {
        SandboxResult r;
        r.success = false;
        r.error   = "MockRuntime: module not loaded";
        return r;
    }

    // Return queued result if available, else a default success.
    if (!result_queue_.empty()) {
        SandboxResult r = result_queue_.front();
        result_queue_.pop();
        return r;
    }

    // Default: successful execution with empty output.
    SandboxResult r;
    r.success = true;
    r.output  = "{}";
    return r;
}

bool MockRuntime::is_loaded() const { return loaded_; }

void MockRuntime::reload() {
    // In mock, reload simply sets loaded back to true.
    loaded_ = true;
}

void MockRuntime::set_next_result(const SandboxResult& result) {
    result_queue_.push(result);
}

void MockRuntime::simulate_violation(SandboxViolation::Type type,
                                     const std::string& details) {
    SandboxResult r;
    r.success   = false;
    r.error     = "sandbox violation";
    r.violation = SandboxViolation{type, details};
    result_queue_.push(r);
}

void MockRuntime::set_loaded(bool loaded) { loaded_ = loaded; }

size_t      MockRuntime::call_count()          const { return call_count_; }
const std::string& MockRuntime::last_function_name() const { return last_function_; }
const std::string& MockRuntime::last_params_json()   const { return last_params_; }

void MockRuntime::reset() {
    call_count_ = 0;
    last_function_.clear();
    last_params_.clear();
    // Clear the queue
    std::queue<SandboxResult> empty;
    result_queue_.swap(empty);
}

// ============================================================================
// WasmExecutor
// ============================================================================

WasmExecutor::WasmExecutor(std::unique_ptr<IWasmRuntime> runtime,
                           const std::string& module_path)
    : runtime_(std::move(runtime))
    , module_path_(module_path) {
    if (!runtime_) {
        throw std::invalid_argument("WasmExecutor: runtime must not be null");
    }
}

SandboxResult WasmExecutor::execute(const std::string& function_name,
                                    const std::string& params_json,
                                    const SandboxConfig& config) {
    if (!runtime_->is_loaded()) {
        SandboxResult r;
        r.success = false;
        r.error   = "WasmExecutor: runtime not loaded";
        return r;
    }

    // Record wall-clock time around the execution.
    auto start = std::chrono::steady_clock::now();

    SandboxResult result = runtime_->execute(function_name, params_json);

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          end - start).count();

    result.execution_time_ms = static_cast<uint32_t>(elapsed_ms);

    // If the runtime didn't already report a timeout but wall-clock exceeds
    // the config timeout, synthesise a TIMEOUT violation.
    if (config.timeout_ms > 0 &&
        result.execution_time_ms > config.timeout_ms &&
        !result.violation.has_value()) {
        result.success   = false;
        result.error     = "execution exceeded timeout";
        result.violation  = SandboxViolation{
            SandboxViolation::TIMEOUT,
            "wall-clock " + std::to_string(result.execution_time_ms) +
            " ms > limit " + std::to_string(config.timeout_ms) + " ms"};
    }

    return result;
}

bool WasmExecutor::is_loaded() const {
    return runtime_ && runtime_->is_loaded();
}

void WasmExecutor::reload() {
    if (runtime_) { runtime_->reload(); }
}

const std::string& WasmExecutor::module_path() const {
    return module_path_;
}

// ============================================================================
// SandboxManager — minimal stubs (full implementation in Task 4)
// ============================================================================

void SandboxManager::load_module(const std::string& /*tool_name*/,
                                 const std::string& /*wasm_path*/) {
    // TODO(Task 4): implement with RuntimeFactory and executor caching
}

void SandboxManager::unload_module(const std::string& /*tool_name*/) {
    // TODO(Task 4)
}

bool SandboxManager::has_module(const std::string& /*tool_name*/) const {
    return false; // TODO(Task 4)
}

SandboxResult SandboxManager::execute_tool(const std::string& /*tool_name*/,
                                           const std::string& /*params_json*/,
                                           const SandboxConfig& /*config*/) {
    SandboxResult r;
    r.success = false;
    r.error   = "SandboxManager not yet implemented";
    return r; // TODO(Task 4)
}

void SandboxManager::set_default_config(const SandboxConfig& /*config*/) {
    // TODO(Task 4)
}

SandboxConfig SandboxManager::get_config_for_tool(
        const std::string& /*tool_name*/) const {
    return SandboxConfig::safe_defaults(); // TODO(Task 4)
}

} // namespace guardian
