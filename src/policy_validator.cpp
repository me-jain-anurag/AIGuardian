// src/policy_validator.cpp
// Owner: Dev C (stub implementation by Dev A for integration)
// Policy validation engine
#include "guardian/policy_validator.hpp"
#include "guardian/policy_graph.hpp"

#include <algorithm>
#include <unordered_map>

namespace guardian {

PolicyValidator::PolicyValidator(const PolicyGraph& graph)
    : graph_(graph), config_(), max_cache_size_(100) {}

PolicyValidator::PolicyValidator(const PolicyGraph& graph, const Config& config)
    : graph_(graph), config_(config), max_cache_size_(100) {}

bool PolicyValidator::check_transition(const std::string& from_tool,
                                        const std::string& to_tool) const {
    return graph_.has_edge(from_tool, to_tool);
}

std::optional<CycleInfo> PolicyValidator::detect_cycle(
    const std::vector<ToolCall>& action_sequence) const {
    std::unordered_map<std::string, uint32_t> counts;
    for (const auto& call : action_sequence) {
        counts[call.tool_name]++;
        if (counts[call.tool_name] >= cycle_threshold_) {
            CycleInfo info;
            info.cycle_tools.push_back(call.tool_name);
            info.cycle_length = counts[call.tool_name];
            return info;
        }
    }
    return std::nullopt;
}

std::optional<ExfiltrationPath> PolicyValidator::detect_exfiltration(
    const std::vector<ToolCall>& action_sequence) const {
    for (const auto& call : action_sequence) {
        auto node = graph_.get_node(call.tool_name);
        if (node && node->node_type == NodeType::SENSITIVE_SOURCE) {
            for (const auto& other : action_sequence) {
                auto dest = graph_.get_node(other.tool_name);
                if (dest && dest->node_type == NodeType::EXTERNAL_DESTINATION) {
                    ExfiltrationPath path;
                    path.source_node = call.tool_name;
                    path.destination_node = other.tool_name;
                    path.path = {call.tool_name, other.tool_name};
                    return path;
                }
            }
        }
    }
    return std::nullopt;
}

bool PolicyValidator::is_valid_path(const std::vector<std::string>& path) const {
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (!graph_.has_edge(path[i], path[i + 1])) return false;
    }
    return true;
}

std::vector<std::string> PolicyValidator::get_alternatives(const std::string& from_tool) const {
    std::vector<std::string> alts;
    auto neighbors = graph_.get_neighbors(from_tool);
    for (const auto& edge : neighbors) {
        alts.push_back(edge.to_node_id);
    }
    return alts;
}

std::string PolicyValidator::generate_cache_key(
    const std::string& tool_name,
    const std::vector<ToolCall>& sequence,
    size_t context_length) {
    std::string key = tool_name + ":";
    size_t start = sequence.size() > context_length ? sequence.size() - context_length : 0;
    for (size_t i = start; i < sequence.size(); ++i) {
        key += sequence[i].tool_name + ",";
    }
    return key;
}

ValidationResult PolicyValidator::validate(
    const std::string& tool_name,
    const std::vector<ToolCall>& action_sequence) const {

    ValidationResult result;

    // Check if tool exists in graph
    if (!graph_.has_node(tool_name)) {
        result.approved = false;
        result.reason = "Unknown tool '" + tool_name + "' (not found in policy graph)";
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
        cycle.cycle_tools.push_back(tool_name);
        cycle.cycle_length = count;
        // cycle.threshold logic handled outside for now
        result.detected_cycle = cycle;
        return result;
    }

    // Check for data exfiltration paths (sensitive_source → external_destination)
    if (node && node->node_type == NodeType::EXTERNAL_DESTINATION) {
        for (const auto& call : action_sequence) {
            auto prev_node = graph_.get_node(call.tool_name);
            if (prev_node && prev_node->node_type == NodeType::SENSITIVE_SOURCE) {
                if (graph_.is_reachable(call.tool_name, tool_name)) {
                    ExfiltrationPath exfil;
                    exfil.source_node = call.tool_name;
                    exfil.destination_node = tool_name;
                    exfil.path = graph_.find_path(call.tool_name, tool_name);
                    result.detected_exfiltration = exfil;

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
