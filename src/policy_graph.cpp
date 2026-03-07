// src/policy_graph.cpp
// Owner: Dev A
// PolicyGraph implementation — adjacency list representation
#include "guardian/policy_graph.hpp"

#include <algorithm>
#include <stdexcept>

namespace guardian {

// ============================================================================
// Node Operations
// ============================================================================

void PolicyGraph::add_node(const PolicyNode& node) {
    if (node.id.empty()) {
        throw std::invalid_argument("PolicyGraph::add_node: node id cannot be empty");
    }
    if (nodes_.count(node.id)) {
        throw std::invalid_argument(
            "PolicyGraph::add_node: node '" + node.id + "' already exists");
    }
    nodes_[node.id] = node;
    // Initialize empty adjacency list entry for this node
    if (!adjacency_list_.count(node.id)) {
        adjacency_list_[node.id] = {};
    }
}

void PolicyGraph::remove_node(const std::string& node_id) {
    if (!nodes_.count(node_id)) {
        throw std::invalid_argument(
            "PolicyGraph::remove_node: node '" + node_id + "' does not exist");
    }

    // Remove all edges FROM this node
    adjacency_list_.erase(node_id);

    // Remove all edges TO this node from other adjacency lists
    for (auto& [src_id, edges] : adjacency_list_) {
        edges.erase(
            std::remove_if(edges.begin(), edges.end(),
                [&node_id](const PolicyEdge& e) {
                    return e.to_node_id == node_id;
                }),
            edges.end());
    }

    nodes_.erase(node_id);
}

bool PolicyGraph::has_node(const std::string& node_id) const {
    return nodes_.count(node_id) > 0;
}

std::optional<PolicyNode> PolicyGraph::get_node(const std::string& node_id) const {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

// ============================================================================
// Edge Operations
// ============================================================================

void PolicyGraph::add_edge(const PolicyEdge& edge) {
    if (edge.from_node_id.empty() || edge.to_node_id.empty()) {
        throw std::invalid_argument(
            "PolicyGraph::add_edge: from_node_id and to_node_id cannot be empty");
    }
    if (!nodes_.count(edge.from_node_id)) {
        throw std::invalid_argument(
            "PolicyGraph::add_edge: source node '" + edge.from_node_id + "' does not exist");
    }
    if (!nodes_.count(edge.to_node_id)) {
        throw std::invalid_argument(
            "PolicyGraph::add_edge: destination node '" + edge.to_node_id + "' does not exist");
    }

    // Check for duplicate edge
    auto& edges = adjacency_list_[edge.from_node_id];
    for (const auto& existing : edges) {
        if (existing.to_node_id == edge.to_node_id) {
            throw std::invalid_argument(
                "PolicyGraph::add_edge: edge from '" + edge.from_node_id +
                "' to '" + edge.to_node_id + "' already exists");
        }
    }

    edges.push_back(edge);
}

void PolicyGraph::remove_edge(const std::string& from_id, const std::string& to_id) {
    auto it = adjacency_list_.find(from_id);
    if (it == adjacency_list_.end()) {
        throw std::invalid_argument(
            "PolicyGraph::remove_edge: source node '" + from_id + "' not found");
    }

    auto& edges = it->second;
    auto original_size = edges.size();
    edges.erase(
        std::remove_if(edges.begin(), edges.end(),
            [&to_id](const PolicyEdge& e) {
                return e.to_node_id == to_id;
            }),
        edges.end());

    if (edges.size() == original_size) {
        throw std::invalid_argument(
            "PolicyGraph::remove_edge: edge from '" + from_id +
            "' to '" + to_id + "' does not exist");
    }
}

bool PolicyGraph::has_edge(const std::string& from_id, const std::string& to_id) const {
    auto it = adjacency_list_.find(from_id);
    if (it == adjacency_list_.end()) {
        return false;
    }
    const auto& edges = it->second;
    return std::any_of(edges.begin(), edges.end(),
        [&to_id](const PolicyEdge& e) {
            return e.to_node_id == to_id;
        });
}

std::vector<PolicyEdge> PolicyGraph::get_neighbors(const std::string& node_id) const {
    auto it = adjacency_list_.find(node_id);
    if (it == adjacency_list_.end()) {
        return {};
    }
    return it->second;
}

// ============================================================================
// Accessors
// ============================================================================

size_t PolicyGraph::node_count() const {
    return nodes_.size();
}

size_t PolicyGraph::edge_count() const {
    size_t count = 0;
    for (const auto& [node_id, edges] : adjacency_list_) {
        count += edges.size();
    }
    return count;
}

std::vector<std::string> PolicyGraph::get_all_node_ids() const {
    std::vector<std::string> ids;
    ids.reserve(nodes_.size());
    for (const auto& [id, node] : nodes_) {
        ids.push_back(id);
    }
    return ids;
}

// ============================================================================
// Serialization (Task 3 — implemented separately)
// ============================================================================

std::string PolicyGraph::to_json() const {
    // TODO: Task 3 — JSON serialization
    return "{}";
}

PolicyGraph PolicyGraph::from_json(const std::string& /*json_str*/) {
    // TODO: Task 3 — JSON deserialization
    return PolicyGraph{};
}

std::string PolicyGraph::to_dot() const {
    // TODO: Task 3 — DOT serialization
    return "digraph {}";
}

PolicyGraph PolicyGraph::from_dot(const std::string& /*dot_str*/) {
    // TODO: Task 3 — DOT deserialization
    return PolicyGraph{};
}

} // namespace guardian
