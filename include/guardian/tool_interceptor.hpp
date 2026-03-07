// include/guardian/tool_interceptor.hpp
// Owner: Dev C
// Tool call interception coordinating validation and sandbox execution
#pragma once

#include "types.hpp"
#include <string>

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

    // Intercept and validate a tool call
    ValidationResult intercept(const ToolCall& call);

    // Validate and execute if approved
    std::pair<ValidationResult, std::optional<SandboxResult>>
    execute_if_valid(const ToolCall& call);
};

} // namespace guardian
