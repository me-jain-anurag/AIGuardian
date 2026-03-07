// include/guardian/types.hpp — THE SHARED CONTRACT
// All 4 developers code against these types.
// Changes to this file require a PR reviewed by all team members.
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <chrono>
#include <cstdint>

namespace guardian {

// ============================================================================
// Enums
// ============================================================================

enum class RiskLevel {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

enum class NodeType {
    NORMAL,
    SENSITIVE_SOURCE,
    EXTERNAL_DESTINATION,
    DATA_PROCESSOR
};

// ============================================================================
// Sandbox Types (Dev B owns implementation)
// ============================================================================

struct SandboxConfig {
    uint64_t memory_limit_mb = 128;
    uint32_t timeout_ms = 5000;
    std::vector<std::string> allowed_paths;      // empty = no file access
    bool network_access = false;
    std::map<std::string, std::string> environment_vars;

    /// Returns a config with documented safe defaults:
    ///   128 MB memory, 5000 ms timeout, no file access, no network.
    static SandboxConfig safe_defaults() { return SandboxConfig{}; }
};

struct SandboxViolation {
    enum Type {
        MEMORY_EXCEEDED,
        TIMEOUT,
        FILE_ACCESS_DENIED,
        NETWORK_DENIED
    };
    Type type;
    std::string details;
};

struct SandboxResult {
    bool success = false;
    std::string output;
    std::optional<std::string> error;
    uint64_t memory_used_bytes = 0;
    uint32_t execution_time_ms = 0;
    std::optional<SandboxViolation> violation;
};

// ============================================================================
// Tool Call Types (Dev C owns implementation)
// ============================================================================

struct ToolCall {
    std::string tool_name;
    std::map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point timestamp;
    std::string session_id;
};

// ============================================================================
// Validation Types (Dev C owns implementation)
// ============================================================================

struct CycleInfo {
    size_t cycle_start_index = 0;
    size_t cycle_length = 0;
    std::vector<std::string> cycle_tools;
};

struct ExfiltrationPath {
    std::string source_node;
    std::string destination_node;
    std::vector<std::string> path;
};

struct ValidationResult {
    bool approved = false;
    std::string reason;
    std::vector<std::string> suggested_alternatives;
    std::optional<CycleInfo> detected_cycle;
    std::optional<ExfiltrationPath> detected_exfiltration;
};

} // namespace guardian
