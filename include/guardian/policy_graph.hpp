// include/guardian/policy_graph.hpp
// Owner: Dev A
// Policy graph engine with adjacency list representation
#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace guardian {

struct PolicyNode {
    std::string id;
    std::string tool_name;
    RiskLevel risk_level = RiskLevel::LOW;
    NodeType node_type = NodeType::NORMAL;
    std::map<std::string, std::string> metadata;
    std::string wasm_module;
    SandboxConfig sandbox_config;
};

struct PolicyEdge {
    std::string from_node_id;
    std::string to_node_id;
    std::map<std::string, std::string> conditions;
    std::map<std::string, std::string> metadata;
};

class PolicyGraph {
public:
    PolicyGraph() = default;
    ~PolicyGraph() = default;

    // Node operations
    void add_node(const PolicyNode& node);
    void remove_node(const std::string& node_id);
    bool has_node(const std::string& node_id) const;
    std::optional<PolicyNode> get_node(const std::string& node_id) const;

    // Edge operations
    void add_edge(const PolicyEdge& edge);
    void remove_edge(const std::string& from_id, const std::string& to_id);
    bool has_edge(const std::string& from_id, const std::string& to_id) const;
    std::vector<PolicyEdge> get_neighbors(const std::string& node_id) const;

    // Accessors
    size_t node_count() const;
    size_t edge_count() const;
    std::vector<std::string> get_all_node_ids() const;

    // Serialization
    std::string to_json() const;
    static PolicyGraph from_json(const std::string& json_str);
    std::string to_dot() const;
    static PolicyGraph from_dot(const std::string& dot_str);

private:
    std::unordered_map<std::string, PolicyNode> nodes_;
    std::unordered_map<std::string, std::vector<PolicyEdge>> adjacency_list_;
};

} // namespace guardian
