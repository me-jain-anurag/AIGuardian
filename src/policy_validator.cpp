// src/policy_validator.cpp
// Owner: Dev C
// Policy validation engine — Phase 3: LRU caching, optimized cycle/exfiltration
// detection
#include "guardian/policy_validator.hpp"
#include "guardian/policy_graph.hpp"

#include <algorithm>
#include <unordered_map>

namespace guardian {

PolicyValidator::PolicyValidator(const PolicyGraph& graph) : graph_(graph), config_(), validation_cache_(200) {}

PolicyValidator::PolicyValidator(const PolicyGraph& graph, const Config& config)
    : graph_(graph), config_(config), validation_cache_(200) {}

bool PolicyValidator::check_transition(const std::string& from_tool, const std::string& to_tool) const {
    if (from_tool.empty()) {
        // Initial call — any known node is a valid starting point
        return graph_.get_node_by_tool_name(to_tool).has_value();
    }
    return graph_.has_edge(from_tool, to_tool);
}

std::optional<CycleInfo> PolicyValidator::detect_cycle(const std::vector<ToolCall>& action_sequence) const {
    if (action_sequence.empty()) return std::nullopt;

    std::string current_tool = action_sequence[0].tool_name;
    uint32_t consecutive = 1;

    for (size_t i = 1; i < action_sequence.size(); ++i) {
        if (action_sequence[i].tool_name == current_tool) {
            ++consecutive;
        } else {
            current_tool = action_sequence[i].tool_name;
            consecutive = 1;
        }

        // Determine threshold: per-tool override or default
        uint32_t threshold = cycle_threshold_;
        auto it = config_.cycle_detection.per_tool_thresholds.find(current_tool);
        if (it != config_.cycle_detection.per_tool_thresholds.end()) {
            threshold = it->second;
        }

        if (consecutive > threshold) {
            CycleInfo info;
            info.cycle_tools.push_back(current_tool);
            info.cycle_length = consecutive;
            return info;
        }
    }
    return std::nullopt;
}

std::optional<ExfiltrationPath>
PolicyValidator::detect_exfiltration(const std::vector<ToolCall>& action_sequence) const {
    std::vector<std::string> active_sources;

    for (const auto& call : action_sequence) {
        auto node = graph_.get_node_by_tool_name(call.tool_name);
        if (!node)
            continue;

        if (node->node_type == NodeType::SENSITIVE_SOURCE) {
            active_sources.push_back(call.tool_name);
        } else if (node->node_type == NodeType::DATA_PROCESSOR) {
            // Processing sanitizes the data flow
            active_sources.clear();
        } else if (node->node_type == NodeType::EXTERNAL_DESTINATION) {
            if (!active_sources.empty()) {
                ExfiltrationPath path;
                path.source_node = active_sources.front();
                path.destination_node = call.tool_name;
                path.path = {active_sources.front(), call.tool_name};
                return path;
            }
        }
    }
    return std::nullopt;
}

bool PolicyValidator::is_valid_path(const std::vector<std::string>& path) const {
    if (path.empty()) return true;
    // Single node: must exist in the graph
    if (path.size() == 1) {
        return graph_.get_node_by_tool_name(path[0]).has_value();
    }
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (!graph_.has_edge(path[i], path[i + 1]))
            return false;
    }
    return true;
}

std::vector<std::string> PolicyValidator::get_alternatives(const std::string& from_tool) const {
    std::vector<std::string> alts;
    auto neighbors = graph_.get_neighbors(from_tool);
    alts.reserve(neighbors.size());
    for (const auto& edge : neighbors) {
        alts.push_back(edge.to_node_id);
    }
    return alts;
}

std::string PolicyValidator::generate_cache_key(const std::string& tool_name, const std::vector<ToolCall>& sequence,
                                                size_t context_length) {
    std::string key;
    key.reserve(tool_name.size() + context_length * 16);
    key += tool_name;
    key += ':';
    size_t start = sequence.size() > context_length ? sequence.size() - context_length : 0;
    for (size_t i = start; i < sequence.size(); ++i) {
        key += sequence[i].tool_name;
        key += ',';
    }
    return key;
}

ValidationResult PolicyValidator::validate(const std::string& tool_name,
                                           const std::vector<ToolCall>& action_sequence) const {

    // Phase 3: LRU cache lookup
    std::string cache_key = generate_cache_key(tool_name, action_sequence);
    auto cached = validation_cache_.get(cache_key);
    if (cached) {
        ++cache_hits_;
        return *cached;
    }
    ++cache_misses_;

    ValidationResult result;

    // Phase 3: Fast tool_name to node_id lookup
    auto node_opt = graph_.get_node_by_tool_name(tool_name);
    if (!node_opt) {
        result.approved = false;
        result.reason = "Unknown tool '" + tool_name + "' is not defined in the policy graph";
        result.suggested_alternatives = graph_.get_all_node_ids();
        validation_cache_.put(cache_key, result);
        return result;
    }
    const auto& node = *node_opt;
    const std::string& target_node_id = node.id;

    // Check sequence validity — if there's a previous action, verify edge exists
    if (!action_sequence.empty()) {
        const auto& last_tool = action_sequence.back().tool_name;
        auto prev_node_opt = graph_.get_node_by_tool_name(last_tool);
        if (prev_node_opt && !graph_.has_edge(prev_node_opt->id, target_node_id)) {
            result.approved = false;
            result.reason = "Transition from '" + last_tool + "' to '" + tool_name + "' is not allowed by policy";
            auto neighbors = graph_.get_neighbors(prev_node_opt->id);
            for (const auto& edge : neighbors) {
                // Return tool names, not node IDs
                if (auto n = graph_.get_node(edge.to_node_id)) {
                    result.suggested_alternatives.push_back(n->tool_name);
                }
            }
            validation_cache_.put(cache_key, result);
            return result;
        }
    }

    // Check cycle detection — optimized with early-exit counter
    uint32_t count = 0;
    for (const auto& call : action_sequence) {
        if (call.tool_name == tool_name) {
            ++count;
            if (count >= cycle_threshold_)
                break; // Early exit
        }
    }
    if (count >= cycle_threshold_) {
        result.approved = false;
        result.reason = "Cycle detected: tool '" + tool_name + "' called " + std::to_string(count) +
                        " times (threshold: " + std::to_string(cycle_threshold_) + ")";
        CycleInfo cycle;
        cycle.cycle_tools.push_back(tool_name);
        cycle.cycle_length = count;
        result.detected_cycle = cycle;
        // Don't cache cycle results — sequence changes every call
        return result;
    }

    // Check for data exfiltration paths
    std::vector<ToolCall> test_sequence = action_sequence;
    ToolCall current_call;
    current_call.tool_name = tool_name;
    current_call.timestamp = std::chrono::system_clock::now();
    test_sequence.push_back(current_call);

    if (auto exfil = detect_exfiltration(test_sequence)) {
        result.approved = false;
        result.reason = "Potential data Exfiltration: " + exfil->source_node + " -> " + exfil->destination_node;
        result.detected_exfiltration = *exfil;
        validation_cache_.put(cache_key, result);
        return result;
    }

    // All checks passed
    result.approved = true;
    result.reason = "Approved - all policy checks passed";
    validation_cache_.put(cache_key, result);
    return result;
}

void PolicyValidator::set_cycle_threshold(uint32_t threshold) {
    cycle_threshold_ = threshold;
}

double PolicyValidator::get_cache_hit_rate() const {
    size_t total = cache_hits_.load() + cache_misses_.load();
    if (total == 0)
        return 0.0;
    return static_cast<double>(cache_hits_.load()) / static_cast<double>(total);
}

void PolicyValidator::reset_cache_stats() {
    cache_hits_ = 0;
    cache_misses_ = 0;
    validation_cache_.clear();
}

} // namespace guardian
