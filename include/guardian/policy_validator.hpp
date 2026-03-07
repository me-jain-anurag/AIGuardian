// include/guardian/policy_validator.hpp
// Owner: Dev C (interface updated by Dev A for integration)
// Policy validation with transition, cycle, and exfiltration checks
#pragma once

#include "types.hpp"
#include "policy_graph.hpp"
#include <string>
#include <vector>

namespace guardian {

class PolicyValidator {
public:
    explicit PolicyValidator(const PolicyGraph& graph)
        : graph_(graph), cycle_threshold_(10) {}
    ~PolicyValidator() = default;

    // Main validation
    ValidationResult validate(const std::string& tool_name,
                               const std::vector<ToolCall>& action_sequence) const;

    // Individual checks
    bool check_transition(const std::string& from_tool,
                           const std::string& to_tool) const;
    std::optional<CycleInfo> detect_cycle(
        const std::vector<ToolCall>& action_sequence) const;
    std::optional<ExfiltrationPath> detect_exfiltration(
        const std::vector<ToolCall>& action_sequence) const;

    // Path validation
    bool is_valid_path(const std::vector<std::string>& path) const;

    // Configuration
    void set_cycle_threshold(uint32_t threshold);

private:
    const PolicyGraph& graph_;
    uint32_t cycle_threshold_;
};

} // namespace guardian
