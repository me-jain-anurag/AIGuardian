// include/guardian/tool_interceptor.hpp
// Tool call interception coordinating validation and sandbox execution
#pragma once

#include "types.hpp"
#include <string>
#include <map>
#include <optional>
#include <utility>
#include <nlohmann/json.hpp>

namespace guardian {

// Forward declarations
class PolicyValidator;
class SessionManager;
class SandboxManager;

class ToolInterceptor {
public:
    ToolInterceptor(PolicyValidator* validator,
                    SessionManager* session_mgr,
                    SandboxManager* sandbox_mgr);
    ~ToolInterceptor() = default;

    /// Intercepts a tool call, validates it, and executes if approved.
    std::pair<ValidationResult, std::optional<SandboxResult>> intercept(
        const std::string& session_id,
        const std::string& tool_name,
        const std::map<std::string, std::string>& params);

    /// Template matching Guardian::execute_if_valid (if needed for direct function calls)
    template <typename Func, typename... Args>
    std::pair<ValidationResult, std::optional<SandboxResult>> execute_if_valid(
        const std::string& session_id,
        const std::string& tool_name,
        const std::map<std::string, std::string>& params,
        Func&& func,
        Args&&... args) {
        
        // 1. Validation phase
        auto result = intercept_validation_only(session_id, tool_name, params);
        
        // 2. Execution phase
        if (result.approved) {
            std::forward<Func>(func)(std::forward<Args>(args)...);
            return {result, std::nullopt};
        }
        
        return {result, std::nullopt};
    }

private:
    ValidationResult intercept_validation_only(
        const std::string& session_id,
        const std::string& tool_name,
        const std::map<std::string, std::string>& params);

    PolicyValidator* validator_;
    SessionManager* session_mgr_;
    SandboxManager* sandbox_mgr_;
};

} // namespace guardian
