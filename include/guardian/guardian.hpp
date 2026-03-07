// include/guardian/guardian.hpp
// Owner: Dev A (Phase 2)
// Main entry point API wrapping all components
#pragma once

#include "types.hpp"
#include <string>
#include <memory>

namespace guardian {

// Forward declarations
class PolicyGraph;
class PolicyValidator;
class SessionManager;
class SandboxManager;
class VisualizationEngine;

class Guardian {
public:
    // Construction
    Guardian(const std::string& policy_file_path,
             const std::string& wasm_tools_dir);
    ~Guardian();

    // Main API
    std::pair<ValidationResult, std::optional<SandboxResult>>
    execute_tool(const std::string& tool_name,
                  const std::map<std::string, std::string>& params,
                  const std::string& session_id);

    ValidationResult validate_tool_call(const std::string& tool_name,
                                         const std::string& session_id);

    // Session management
    std::string create_session();
    void end_session(const std::string& session_id);

    // Sandbox management
    void load_tool_module(const std::string& tool_name,
                           const std::string& wasm_path);
    void set_default_sandbox_config(const SandboxConfig& config);

    // Visualization
    std::string visualize_policy(const std::string& format = "dot") const;
    std::string visualize_session(const std::string& session_id,
                                    const std::string& format = "dot") const;
};

} // namespace guardian
