// include/guardian/sandbox_manager.hpp
// Owner: Dev B
// WasmEdge sandbox manager with runtime abstraction
#pragma once

#include "types.hpp"
#include <string>
#include <memory>

namespace guardian {

// Runtime abstraction — Dev B implements WasmEdgeRuntime and MockRuntime
class IWasmRuntime {
public:
    virtual ~IWasmRuntime() = default;
    virtual SandboxResult execute(const std::string& function_name,
                                   const std::string& params_json) = 0;
    virtual bool is_loaded() const = 0;
    virtual void reload() = 0;
};

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
