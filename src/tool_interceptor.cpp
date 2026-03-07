// src/tool_interceptor.cpp
// Owner: Dev C
// Coordinates validation + sandbox execution
#include "guardian/tool_interceptor.hpp"
#include "guardian/logger.hpp"
#include "guardian/policy_validator.hpp"
#include "guardian/sandbox_manager.hpp"
#include "guardian/session_manager.hpp"
#include <chrono>

namespace guardian {

ToolInterceptor::ToolInterceptor(PolicyValidator *validator,
                                 SessionManager *session_mgr,
                                 SandboxManager *sandbox_mgr)
    : validator_(validator), session_mgr_(session_mgr),
      sandbox_mgr_(sandbox_mgr) {}

ValidationResult ToolInterceptor::intercept_validation_only(
    const std::string &session_id, const std::string &tool_name,
    const std::map<std::string, std::string> &params) {

  // 1. Get current sequence
  std::vector<ToolCall> sequence;
  if (session_mgr_->has_session(session_id)) {
    sequence = session_mgr_->get_sequence(session_id);
  } else {
    return ValidationResult{
        false, "Invalid session ID: " + session_id, {}, {}, {}};
  }

  // 2. Validate move
  ValidationResult result = validator_->validate(tool_name, sequence);

  Logger::instance().log_with_context(result.approved ? LogLevel::INFO : LogLevel::WARN,
                         "ToolInterceptor",
                         "validate(" + tool_name + "): " + result.reason,
                         tool_name, session_id);

  // 3. If approved, append to session even if there's no sandbox execution yet
  // This allows execute_if_valid to track the tool call
  if (result.approved) {
    ToolCall call;
    call.tool_name = tool_name;
    call.parameters = params;
    call.timestamp = std::chrono::system_clock::now();
    call.session_id = session_id;
    session_mgr_->append_tool_call(session_id, call);
  }

  return result;
}

std::pair<ValidationResult, std::optional<SandboxResult>>
ToolInterceptor::intercept(const std::string &session_id,
                           const std::string &tool_name,
                           const std::map<std::string, std::string> &params) {

  // 1. Get current sequence
  std::vector<ToolCall> sequence;
  if (session_mgr_->has_session(session_id)) {
    sequence = session_mgr_->get_sequence(session_id);
  } else {
    return {ValidationResult{
                false, "Invalid session ID: " + session_id, {}, {}, {}},
            std::nullopt};
  }

  // 2. Validate using PolicyValidator
  ValidationResult validation = validator_->validate(tool_name, sequence);

  Logger::instance().log_with_context(validation.approved ? LogLevel::INFO : LogLevel::WARN,
                         "ToolInterceptor",
                         "validate(" + tool_name + "): " + validation.reason,
                         tool_name, session_id);

  if (!validation.approved) {
    return {validation, std::nullopt};
  }

  // 3. Execute in SandboxManager
  std::optional<SandboxResult> sandbox_result = std::nullopt;
  bool execution_success = true; // Assume success if no sandbox exists

  if (sandbox_mgr_) {
    // Convert map to JSON string for the sandbox
    nlohmann::json params_json(params);
    sandbox_result = sandbox_mgr_->execute_tool(tool_name, params_json.dump());
    if (sandbox_result) {
      execution_success = sandbox_result->success;
    } else {
      execution_success = false;
    }
  }

  // 4. Append to session manager only if validation passed AND execution
  // succeeded
  if (execution_success) {
    ToolCall call;
    call.tool_name = tool_name;
    call.parameters = params;
    call.timestamp = std::chrono::system_clock::now();
    call.session_id = session_id;
    session_mgr_->append_tool_call(session_id, call);
  } else if (!sandbox_result || !sandbox_result->success) {
    // Validation approved but sandbox execution failed.
    // We override the validation result to reflect the execution failure.
    validation.approved = false;
    validation.reason =
        "Execution blocked: sandbox execution failed or errored.";
  }

  return {validation, sandbox_result};
}

} // namespace guardian
