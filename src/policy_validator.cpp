// src/policy_validator.cpp
// Owner: Dev C (stub implementation by Dev A for integration)
// Policy validation engine
#include "guardian/policy_validator.hpp"
#include "guardian/policy_graph.hpp"

#include <algorithm>

namespace guardian {

ValidationResult PolicyValidator::validate(
    const std::string& tool_name,
    const std::vector<ToolCall>& action_sequence) const {

    ValidationResult result;

    // Check if tool exists in graph
    if (!graph_.has_node(tool_name)) {
        result.approved = false;
        result.reason = "Tool '" + tool_name + "' not found in policy graph";
        result.suggested_alternatives = graph_.get_all_node_ids();
        return result;
    }

    auto node = graph_.get_node(tool_name);

    // Check sequence validity — if there's a previous action, verify edge exists
    if (!action_sequence.empty()) {
        const auto& last_tool = action_sequence.back().tool_name;
        if (graph_.has_node(last_tool) && !graph_.has_edge(last_tool, tool_name)) {
            result.approved = false;
            result.reason = "Transition from '" + last_tool + "' to '" +
                            tool_name + "' is not allowed by policy";
            // Suggest valid next tools
            auto neighbors = graph_.get_neighbors(last_tool);
            for (const auto& edge : neighbors) {
                result.suggested_alternatives.push_back(edge.to_node_id);
            }
            return result;
        }
    }

    // Check cycle detection — count occurrences of this tool
    uint32_t count = 0;
    for (const auto& call : action_sequence) {
        if (call.tool_name == tool_name) ++count;
    }
    if (count >= cycle_threshold_) {
        result.approved = false;
        result.reason = "Cycle detected: tool '" + tool_name +
                        "' called " + std::to_string(count) + " times (threshold: " +
                        std::to_string(cycle_threshold_) + ")";
        CycleInfo cycle;
        cycle.tool_name = tool_name;
        cycle.count = count;
        cycle.threshold = cycle_threshold_;
        result.cycles.push_back(cycle);
        return result;
    }

    // Check for data exfiltration paths (sensitive_source → external_destination)
    if (node && node->node_type == NodeType::EXTERNAL_DESTINATION) {
        for (const auto& call : action_sequence) {
            auto prev_node = graph_.get_node(call.tool_name);
            if (prev_node && prev_node->node_type == NodeType::SENSITIVE_SOURCE) {
                if (graph_.is_reachable(call.tool_name, tool_name)) {
                    ExfiltrationPath exfil;
                    exfil.source_tool = call.tool_name;
                    exfil.destination_tool = tool_name;
                    exfil.path = graph_.find_path(call.tool_name, tool_name);
                    exfil.risk_level = RiskLevel::CRITICAL;
                    result.exfiltration_paths.push_back(exfil);

                    result.approved = false;
                    result.reason = "Potential data exfiltration: " +
                                    call.tool_name + " → " + tool_name;
                    return result;
                }
            }
        }
    }

    // All checks passed
    result.approved = true;
    result.reason = "Approved — all policy checks passed";
    return result;
}

void PolicyValidator::set_cycle_threshold(uint32_t threshold) {
    cycle_threshold_ = threshold;
}

} // namespace guardian
