// src/policy_validator.cpp
// Owner: Dev C
// Policy validation: transition checks, cycle detection, exfiltration detection,
// path validation, and LRU caching (design.md Algorithms 1-5)
#include "guardian/policy_validator.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace guardian {

// ============================================================================
// Construction
// ============================================================================

PolicyValidator::PolicyValidator(const PolicyGraph& graph, const Config& config)
    : graph_(graph),
      config_(config),
      max_cache_size_(config.performance.cache_size) {}

PolicyValidator::PolicyValidator(const PolicyGraph& graph)
    : graph_(graph),
      config_(),   // default config
      max_cache_size_(1000) {}

// ============================================================================
// Algorithm 1: Transition Validation
// ============================================================================

bool PolicyValidator::check_transition(const std::string& from_tool,
                                        const std::string& to_tool) const {
    // Initial tool call (no previous tool) — allow any tool in graph
    if (from_tool.empty()) {
        return graph_.has_node(to_tool);
    }

    // Check if edge exists in the policy graph
    return graph_.has_edge(from_tool, to_tool);
}

std::vector<std::string> PolicyValidator::get_alternatives(
    const std::string& from_tool) const {
    std::vector<std::string> alternatives;
    if (from_tool.empty()) {
        // All nodes are valid first calls
        return graph_.get_all_node_ids();
    }
    auto neighbors = graph_.get_neighbors(from_tool);
    alternatives.reserve(neighbors.size());
    for (const auto& edge : neighbors) {
        alternatives.push_back(edge.to_node_id);
    }
    return alternatives;
}

// ============================================================================
// Algorithm 2: Cycle Detection (frequency map approach)
// ============================================================================

std::optional<CycleInfo> PolicyValidator::detect_cycle(
    const std::vector<ToolCall>& action_sequence) const {

    if (action_sequence.size() < 2) {
        return std::nullopt;
    }

    // Build frequency map: tool_name → list of indices where it appears
    std::unordered_map<std::string, std::vector<size_t>> frequency_map;
    for (size_t i = 0; i < action_sequence.size(); ++i) {
        frequency_map[action_sequence[i].tool_name].push_back(i);
    }

    // Check for consecutive repetitions exceeding threshold
    for (const auto& [tool, indices] : frequency_map) {
        // Get threshold for this tool (per-tool or default)
        uint32_t threshold = config_.cycle_detection.default_threshold;
        auto it = config_.cycle_detection.per_tool_thresholds.find(tool);
        if (it != config_.cycle_detection.per_tool_thresholds.end()) {
            threshold = it->second;
        }

        // Count consecutive occurrences
        size_t consecutive_count = 1;
        for (size_t j = 1; j < indices.size(); ++j) {
            if (indices[j] == indices[j - 1] + 1) {
                consecutive_count++;
                if (consecutive_count > threshold) {
                    CycleInfo info;
                    info.cycle_start_index = indices[j - consecutive_count + 1];
                    info.cycle_length = consecutive_count;
                    info.cycle_tools = {tool};
                    return info;
                }
            } else {
                consecutive_count = 1;
            }
        }
    }

    return std::nullopt;
}

// ============================================================================
// Algorithm 3: Exfiltration Path Detection
// ============================================================================

std::optional<ExfiltrationPath> PolicyValidator::detect_exfiltration(
    const std::vector<ToolCall>& action_sequence) const {

    if (action_sequence.empty()) {
        return std::nullopt;
    }

    // Scan sequence for SENSITIVE_SOURCE → EXTERNAL_DESTINATION patterns
    for (size_t i = 0; i < action_sequence.size(); ++i) {
        const auto& current_tool = action_sequence[i].tool_name;

        // Check if current tool is a sensitive source
        auto source_node = graph_.get_node(current_tool);
        if (!source_node || source_node->node_type != NodeType::SENSITIVE_SOURCE) {
            continue;
        }

        // Look ahead for external destinations
        for (size_t j = i + 1; j < action_sequence.size(); ++j) {
            const auto& next_tool = action_sequence[j].tool_name;

            auto dest_node = graph_.get_node(next_tool);
            if (!dest_node || dest_node->node_type != NodeType::EXTERNAL_DESTINATION) {
                continue;
            }

            // Extract the path between source and destination
            std::vector<std::string> path;
            for (size_t k = i; k <= j; ++k) {
                path.push_back(action_sequence[k].tool_name);
            }

            // Check if path goes through at least one DATA_PROCESSOR
            bool has_processor = false;
            for (size_t k = i + 1; k < j; ++k) {
                auto mid_node = graph_.get_node(action_sequence[k].tool_name);
                if (mid_node && mid_node->node_type == NodeType::DATA_PROCESSOR) {
                    has_processor = true;
                    break;
                }
            }

            // Direct path (no processor) = exfiltration attempt
            if (!has_processor) {
                ExfiltrationPath exfil;
                exfil.source_node = current_tool;
                exfil.destination_node = next_tool;
                exfil.path = path;
                return exfil;
            }
        }
    }

    return std::nullopt;
}

// ============================================================================
// Algorithm 4: Path Validation
// ============================================================================

bool PolicyValidator::is_valid_path(const std::vector<std::string>& path) const {
    if (path.empty()) return true;
    if (path.size() == 1) return graph_.has_node(path[0]);

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (!graph_.has_edge(path[i], path[i + 1])) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Algorithm 5: Main Validate with LRU Caching
// ============================================================================

std::string PolicyValidator::generate_cache_key(
    const std::string& tool_name,
    const std::vector<ToolCall>& sequence,
    size_t context_length) {
    std::ostringstream oss;
    oss << tool_name << "|";

    // Use last N tools from sequence as context
    size_t start = (sequence.size() > context_length)
                       ? sequence.size() - context_length
                       : 0;
    for (size_t i = start; i < sequence.size(); ++i) {
        if (i > start) oss << ",";
        oss << sequence[i].tool_name;
    }
    return oss.str();
}

ValidationResult PolicyValidator::validate(
    const std::string& tool_name,
    const std::vector<ToolCall>& action_sequence) {

    // --- Cache lookup ---
    std::string cache_key = generate_cache_key(tool_name, action_sequence);
    auto cache_it = cache_map_.find(cache_key);
    if (cache_it != cache_map_.end()) {
        // Move to front (most recently used)
        cache_list_.splice(cache_list_.begin(), cache_list_, cache_it->second);
        return cache_it->second->result;
    }

    // --- Perform validation ---
    ValidationResult result;
    result.approved = true;
    result.reason = "Approved";

    // 1. Check if the tool exists in the graph at all
    if (!graph_.has_node(tool_name)) {
        result.approved = false;
        result.reason = "Tool '" + tool_name + "' not defined in policy graph";
        // No alternatives for unknown tool
    }

    // 2. Transition validation
    if (result.approved) {
        std::string from_tool;
        if (!action_sequence.empty()) {
            from_tool = action_sequence.back().tool_name;
        }

        if (!check_transition(from_tool, tool_name)) {
            result.approved = false;
            auto alternatives = get_alternatives(from_tool);
            result.suggested_alternatives = alternatives;

            std::string alt_str;
            for (const auto& a : alternatives) {
                if (!alt_str.empty()) alt_str += ", ";
                alt_str += a;
            }
            result.reason = "Transition from '" + from_tool + "' to '" +
                            tool_name + "' not allowed. Valid next tools: [" +
                            alt_str + "]";
        }
    }

    // 3. Cycle detection
    if (result.approved) {
        // Build temporary sequence including the proposed tool
        std::vector<ToolCall> extended_seq = action_sequence;
        ToolCall proposed;
        proposed.tool_name = tool_name;
        proposed.timestamp = std::chrono::system_clock::now();
        extended_seq.push_back(proposed);

        auto cycle = detect_cycle(extended_seq);
        if (cycle) {
            result.approved = false;
            result.detected_cycle = cycle;
            result.reason = "Cycle detected: tool '" + cycle->cycle_tools[0] +
                            "' repeated " + std::to_string(cycle->cycle_length) +
                            " consecutive times (threshold exceeded)";
        }
    }

    // 4. Exfiltration detection
    if (result.approved) {
        std::vector<ToolCall> extended_seq = action_sequence;
        ToolCall proposed;
        proposed.tool_name = tool_name;
        proposed.timestamp = std::chrono::system_clock::now();
        extended_seq.push_back(proposed);

        auto exfil = detect_exfiltration(extended_seq);
        if (exfil) {
            result.approved = false;
            result.detected_exfiltration = exfil;

            std::string path_str;
            for (const auto& p : exfil->path) {
                if (!path_str.empty()) path_str += " -> ";
                path_str += p;
            }
            result.reason = "Exfiltration path detected: " + path_str +
                            ". Direct path from sensitive source '" +
                            exfil->source_node + "' to external destination '" +
                            exfil->destination_node +
                            "' without data processing";

            // Suggest safe alternatives (DATA_PROCESSOR nodes)
            auto all_nodes = graph_.get_all_node_ids();
            for (const auto& nid : all_nodes) {
                auto node = graph_.get_node(nid);
                if (node && node->node_type == NodeType::DATA_PROCESSOR) {
                    result.suggested_alternatives.push_back(nid);
                }
            }
        }
    }

    // If approved, set descriptive reason
    if (result.approved) {
        if (action_sequence.empty()) {
            result.reason = "Initial tool call allowed";
        } else {
            result.reason = "Transition allowed: " +
                            action_sequence.back().tool_name + " -> " + tool_name;
        }
    }

    // --- Cache insertion with LRU eviction ---
    CacheEntry entry{cache_key, result};
    cache_list_.push_front(entry);
    cache_map_[cache_key] = cache_list_.begin();

    if (cache_map_.size() > max_cache_size_) {
        auto last = cache_list_.end();
        --last;
        cache_map_.erase(last->key);
        cache_list_.pop_back();
    }

    return result;
}

} // namespace guardian
