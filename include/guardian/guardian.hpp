// include/guardian/guardian.hpp
// Owner: Dev A (Phase 2)
// Main entry point API wrapping all components
#pragma once

#include "types.hpp"
#include "policy_graph.hpp"
#include "config.hpp"
#include <string>
#include <memory>
#include <map>
#include <optional>

namespace guardian {

// Forward declarations
class PolicyValidator;
class SessionManager;
class SandboxManager;
class VisualizationEngine;

class Guardian {
public:
    // Construction
    explicit Guardian(const std::string& policy_file_path,
                      const std::string& wasm_tools_dir = "",
                      const std::string& config_file_path = "");
    ~Guardian();

    // Prevent copying (owns unique_ptrs)
    Guardian(const Guardian&) = delete;
    Guardian& operator=(const Guardian&) = delete;

    // Move is OK
    Guardian(Guardian&&) noexcept;
    Guardian& operator=(Guardian&&) noexcept;

    // === Main API ===

    // Validate + execute: returns (validation_result, optional sandbox_result)
    std::pair<ValidationResult, std::optional<SandboxResult>>
    execute_tool(const std::string& tool_name,
                  const std::map<std::string, std::string>& params,
                  const std::string& session_id);

    // Policy-only validation (no sandbox execution)
    ValidationResult validate_tool_call(const std::string& tool_name,
                                         const std::string& session_id);

    // === Session Management ===
    std::string create_session();
    void end_session(const std::string& session_id);

    // === Sandbox Management ===
    void load_tool_module(const std::string& tool_name,
                           const std::string& wasm_path);
    void set_default_sandbox_config(const SandboxConfig& config);

    // === Visualization ===
    std::string visualize_policy(const std::string& format = "dot") const;
    std::string visualize_session(const std::string& session_id,
                                    const std::string& format = "dot") const;

    // === Configuration ===
    void update_config(const Config& config);
    Config get_config() const;

    // === Accessors ===
    const PolicyGraph& get_policy_graph() const;
    bool is_initialized() const;

private:
    PolicyGraph policy_graph_;
    Config config_;
    bool initialized_ = false;

    // Component instances
    std::unique_ptr<SessionManager> session_mgr_;
    std::unique_ptr<PolicyValidator> validator_;
    std::unique_ptr<VisualizationEngine> viz_;
    std::unique_ptr<SandboxManager> sandbox_mgr_;
};

} // namespace guardian
