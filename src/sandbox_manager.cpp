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
// WasmEdgeRuntime (Linux only — guarded by HAVE_WASMEDGE)
// ============================================================================

#ifdef HAVE_WASMEDGE

#include <wasmedge/wasmedge.h>
#include <fstream>
#include <sstream>

// PIMPL hides all WasmEdge C types from the header.
struct WasmEdgeRuntime::Impl {
    WasmEdge_ConfigureContext* conf_cxt   = nullptr;
    WasmEdge_VMContext*        vm_cxt     = nullptr;
    WasmEdge_StoreContext*     store_cxt  = nullptr;

    ~Impl() {
        if (vm_cxt)    WasmEdge_VMDelete(vm_cxt);
        if (conf_cxt)  WasmEdge_ConfigureDelete(conf_cxt);
        // store_cxt is owned by the VM — do NOT delete separately.
    }
};

// -- helpers ----------------------------------------------------------------

/// Convert megabytes to WebAssembly pages (1 page = 64 KiB).
static uint32_t mb_to_wasm_pages(uint64_t mb) {
    constexpr uint64_t kBytesPerPage = 65536;  // 64 KiB
    return static_cast<uint32_t>((mb * 1024 * 1024) / kBytesPerPage);
}

// -- constructor / destructor ------------------------------------------------

WasmEdgeRuntime::WasmEdgeRuntime(const std::string& wasm_path,
                                 const SandboxConfig& config)
    : impl_(std::make_unique<Impl>())
    , wasm_path_(wasm_path)
    , config_(config)
{
    // 1. Create configure context and enable WASI.
    impl_->conf_cxt = WasmEdge_ConfigureCreate();
    if (!impl_->conf_cxt) {
        throw std::runtime_error(
            "WasmEdgeRuntime: failed to create configure context");
    }
    WasmEdge_ConfigureAddHostRegistration(
        impl_->conf_cxt, WasmEdge_HostRegistration_Wasi);

    // 2. Apply sandbox constraints.
    apply_config(config);

    // 3. Create VM.
    impl_->vm_cxt = WasmEdge_VMCreate(impl_->conf_cxt, nullptr);
    if (!impl_->vm_cxt) {
        throw std::runtime_error(
            "WasmEdgeRuntime: failed to create VM context");
    }

    // 4. Configure WASI module (preopened dirs, env vars, network).
    {
        // Build C-string arrays for WASI init.
        // -- preopened dirs (file access control, Task 3.5)
        std::vector<const char*> dirs;
        for (const auto& p : config_.allowed_paths) {
            dirs.push_back(p.c_str());
        }

        // -- environment variables
        std::vector<std::string> env_strs;
        std::vector<const char*> envs;
        for (const auto& [k, v] : config_.environment_vars) {
            env_strs.push_back(k + "=" + v);
        }
        for (const auto& e : env_strs) {
            envs.push_back(e.c_str());
        }

        auto* wasi_module = WasmEdge_VMGetImportModuleContext(
            impl_->vm_cxt, WasmEdge_HostRegistration_Wasi);
        if (wasi_module) {
            WasmEdge_ModuleInstanceInitWASI(
                wasi_module,
                nullptr, 0,                                  // args
                envs.empty()  ? nullptr : envs.data(),
                static_cast<uint32_t>(envs.size()),
                dirs.empty()  ? nullptr : dirs.data(),
                static_cast<uint32_t>(dirs.size()));
        }
    }

    // 5. Load the Wasm module.
    load_module();
}

WasmEdgeRuntime::~WasmEdgeRuntime() = default;  // Impl dtor handles cleanup

// -- apply_config -----------------------------------------------------------

void WasmEdgeRuntime::apply_config(const SandboxConfig& config) {
    // Task 3.3 — Memory limit (MB → Wasm pages).
    WasmEdge_ConfigureSetMaxMemoryPage(
        impl_->conf_cxt, mb_to_wasm_pages(config.memory_limit_mb));

    // Task 3.4 — Timeout enforcement.
    if (config.timeout_ms > 0) {
        WasmEdge_ConfigureSetTimelimit(impl_->conf_cxt, config.timeout_ms);
    }

    // Task 3.6 — Network access.
    // WasmEdge/WASI restricts network by default.  Preopened dirs handle
    // file access (Task 3.5); network access requires WASI sockets which
    // are not preopened unless explicitly enabled via the allowed_paths
    // mechanism or the host registration.  When network_access is false
    // (default), we simply do not enable any socket host functions.
    // When true, we would add the WasiSocket registration here.
    if (config.network_access) {
        WasmEdge_ConfigureAddHostRegistration(
            impl_->conf_cxt, WasmEdge_HostRegistration_WasiNN);
        // Note: for full socket support, WasmEdge_HostRegistration_Wasi
        // already provides basic WASI socket via preopened sockets.
        // Advanced socket policy can be layered later.
    }
}

// -- load_module ------------------------------------------------------------

void WasmEdgeRuntime::load_module() {
    // Validate file exists.
    {
        std::ifstream f(wasm_path_, std::ios::binary);
        if (!f.good()) {
            throw std::runtime_error(
                "WasmEdgeRuntime: file not found: " + wasm_path_);
        }
    }

    // Load and validate module in the VM.
    WasmEdge_Result res = WasmEdge_VMLoadWasmFromFile(
        impl_->vm_cxt, wasm_path_.c_str());
    if (!WasmEdge_ResultOK(res)) {
        loaded_ = false;
        throw std::runtime_error(
            "WasmEdgeRuntime: failed to load module: " + wasm_path_ +
            " (" + WasmEdge_ResultGetMessage(res) + ")");
    }

    res = WasmEdge_VMValidate(impl_->vm_cxt);
    if (!WasmEdge_ResultOK(res)) {
        loaded_ = false;
        throw std::runtime_error(
            "WasmEdgeRuntime: module validation failed: " + wasm_path_ +
            " (" + WasmEdge_ResultGetMessage(res) + ")");
    }

    res = WasmEdge_VMInstantiate(impl_->vm_cxt);
    if (!WasmEdge_ResultOK(res)) {
        loaded_ = false;
        throw std::runtime_error(
            "WasmEdgeRuntime: module instantiation failed: " + wasm_path_ +
            " (" + WasmEdge_ResultGetMessage(res) + ")");
    }

    impl_->store_cxt = WasmEdge_VMGetStoreContext(impl_->vm_cxt);
    loaded_ = true;
}

// -- execute ----------------------------------------------------------------

SandboxResult WasmEdgeRuntime::execute(const std::string& function_name,
                                       const std::string& params_json) {
    SandboxResult result;

    if (!loaded_) {
        result.success = false;
        result.error   = "WasmEdgeRuntime: module not loaded";
        return result;
    }

    auto start = std::chrono::steady_clock::now();

    // Prepare function name.
    WasmEdge_String func_name = WasmEdge_StringCreateByCString(
        function_name.c_str());

    // Pass params_json as a single i32 pointer argument pattern.
    // For simplicity, we call the function with no Wasm-level args and
    // expect the tool to read params via WASI stdin / shared memory.
    // Alternatively, if the function accepts (i32, i32) for ptr+len,
    // we would write params_json into linear memory first.
    //
    // For the MVP, we use the "no args, result via stdout" convention:
    // the tool reads params from WASI stdin and writes output to stdout.

    WasmEdge_Value returns[1];
    WasmEdge_Result res = WasmEdge_VMExecute(
        impl_->vm_cxt, func_name, nullptr, 0, returns, 1);

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       end - start).count();
    result.execution_time_ms = static_cast<uint32_t>(elapsed);

    WasmEdge_StringDelete(func_name);

    // Task 3.9 — Resource monitoring: get memory usage from store.
    if (impl_->store_cxt) {
        uint32_t mem_count = WasmEdge_StoreListMemoryLength(impl_->store_cxt);
        if (mem_count > 0) {
            WasmEdge_String mem_names[1];
            WasmEdge_StoreListMemory(impl_->store_cxt, mem_names, 1);
            const WasmEdge_MemoryInstanceContext* mem_cxt =
                WasmEdge_StoreFindMemory(impl_->store_cxt, mem_names[0]);
            if (mem_cxt) {
                uint32_t pages = WasmEdge_MemoryInstanceGetPageSize(mem_cxt);
                result.memory_used_bytes =
                    static_cast<uint64_t>(pages) * 65536;  // 64 KiB/page
            }
            WasmEdge_StringDelete(mem_names[0]);
        }
    }

    // Task 3.9 — Detailed violation reporting.
    if (!WasmEdge_ResultOK(res)) {
        result.success = false;
        std::string msg = WasmEdge_ResultGetMessage(res);

        // Detect violation type from the error message.
        if (msg.find("timeout") != std::string::npos ||
            msg.find("time limit") != std::string::npos) {
            result.error     = "execution timed out";
            result.violation = SandboxViolation{
                SandboxViolation::TIMEOUT,
                "execution exceeded " +
                std::to_string(config_.timeout_ms) + " ms limit"};
        } else if (msg.find("memory") != std::string::npos ||
                   msg.find("out of bounds") != std::string::npos) {
            result.error     = "memory limit exceeded";
            result.violation = SandboxViolation{
                SandboxViolation::MEMORY_EXCEEDED,
                "memory usage exceeded " +
                std::to_string(config_.memory_limit_mb) + " MB limit"};
        } else if (msg.find("permission") != std::string::npos ||
                   msg.find("access denied") != std::string::npos ||
                   msg.find("not allowed") != std::string::npos) {
            // Distinguish file vs network violations.
            if (msg.find("socket") != std::string::npos ||
                msg.find("network") != std::string::npos) {
                result.error     = "network access denied";
                result.violation = SandboxViolation{
                    SandboxViolation::NETWORK_DENIED,
                    "network access is disabled in sandbox config"};
            } else {
                result.error     = "file access denied";
                result.violation = SandboxViolation{
                    SandboxViolation::FILE_ACCESS_DENIED,
                    "path not in allowed_paths: " + msg};
            }
        } else {
            result.error = "execution failed: " + msg;
            // Generic failure — no specific violation type.
        }
        return result;
    }

    // Task 3.7 — Extract result as JSON.
    // Convention: i32 return value of 0 = success, non-zero = error code.
    // Actual output is written to WASI stdout by the tool.
    result.success = true;
    if (WasmEdge_ValTypeIsI32(returns[0].Type)) {
        int32_t ret_code = WasmEdge_ValueGetI32(returns[0]);
        if (ret_code != 0) {
            result.success = false;
            result.error   = "tool returned error code: " +
                             std::to_string(ret_code);
        }
    }
    // The actual output JSON would be captured from WASI stdout redirection.
    // For now, set a placeholder; real stdout capture is wired up when
    // the WASI module is configured with output file descriptors.
    result.output = "{}";

    // Check memory against limit (post-execution, Task 3.9).
    uint64_t mem_limit_bytes = config_.memory_limit_mb * 1024 * 1024;
    if (result.memory_used_bytes > mem_limit_bytes &&
        !result.violation.has_value()) {
        result.success   = false;
        result.error     = "memory limit exceeded";
        result.violation  = SandboxViolation{
            SandboxViolation::MEMORY_EXCEEDED,
            "used " + std::to_string(result.memory_used_bytes / (1024*1024)) +
            " MB, limit " + std::to_string(config_.memory_limit_mb) + " MB"};
    }

    return result;
}

// -- is_loaded / reload -----------------------------------------------------

bool WasmEdgeRuntime::is_loaded() const { return loaded_; }

void WasmEdgeRuntime::reload() {
    // Task 3.8 — Hot-swap: destroy old VM, re-create with same config.
    loaded_ = false;

    // Clean up old VM.
    if (impl_->vm_cxt) {
        WasmEdge_VMDelete(impl_->vm_cxt);
        impl_->vm_cxt   = nullptr;
        impl_->store_cxt = nullptr;
    }

    // Re-create VM and reload module.
    impl_->vm_cxt = WasmEdge_VMCreate(impl_->conf_cxt, nullptr);
    if (!impl_->vm_cxt) {
        throw std::runtime_error(
            "WasmEdgeRuntime::reload: failed to create VM context");
    }

    // Re-configure WASI with same settings.
    {
        std::vector<const char*> dirs;
        for (const auto& p : config_.allowed_paths) {
            dirs.push_back(p.c_str());
        }
        std::vector<std::string> env_strs;
        std::vector<const char*> envs;
        for (const auto& [k, v] : config_.environment_vars) {
            env_strs.push_back(k + "=" + v);
        }
        for (const auto& e : env_strs) {
            envs.push_back(e.c_str());
        }
        auto* wasi_module = WasmEdge_VMGetImportModuleContext(
            impl_->vm_cxt, WasmEdge_HostRegistration_Wasi);
        if (wasi_module) {
            WasmEdge_ModuleInstanceInitWASI(
                wasi_module,
                nullptr, 0,
                envs.empty() ? nullptr : envs.data(),
                static_cast<uint32_t>(envs.size()),
                dirs.empty() ? nullptr : dirs.data(),
                static_cast<uint32_t>(dirs.size()));
        }
    }

    load_module();
}

#endif // HAVE_WASMEDGE

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
