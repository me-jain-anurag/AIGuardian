// include/guardian/policy_validator.hpp
// Owner: Dev C
// Policy validation with transition, cycle, exfiltration checks, and LRU caching
#pragma once

#include "types.hpp"
#include "policy_graph.hpp"
#include "config.hpp"
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <optional>

namespace guardian {

class PolicyValidator {
public:
    /// Construct with graph and config (cycle thresholds, cache size, etc.)
    PolicyValidator(const PolicyGraph& graph, const Config& config);

    /// Construct with graph only (uses default config)
    explicit PolicyValidator(const PolicyGraph& graph);

    ~PolicyValidator() = default;

    // Main validation — coordinates all checks, with LRU caching
    ValidationResult validate(const std::string& tool_name,
                               const std::vector<ToolCall>& action_sequence);

    // Individual checks
    bool check_transition(const std::string& from_tool,
                           const std::string& to_tool) const;
    std::optional<CycleInfo> detect_cycle(
        const std::vector<ToolCall>& action_sequence) const;
    std::optional<ExfiltrationPath> detect_exfiltration(
        const std::vector<ToolCall>& action_sequence) const;

    // Path validation
    bool is_valid_path(const std::vector<std::string>& path) const;

    // Suggest valid next tools from current position
    std::vector<std::string> get_alternatives(const std::string& from_tool) const;

private:
    /// Generate cache key from tool name + last N tools in sequence
    static std::string generate_cache_key(
        const std::string& tool_name,
        const std::vector<ToolCall>& sequence,
        size_t context_length = 5);

    const PolicyGraph& graph_;
    Config config_;

    // LRU validation cache: key → ValidationResult
    struct CacheEntry {
        std::string key;
        ValidationResult result;
    };
    std::list<CacheEntry> cache_list_;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> cache_map_;
    size_t max_cache_size_;
};

} // namespace guardian
