// src/policy_graph.cpp
// PolicyGraph implementation — adjacency list + JSON/DOT serialization
#include "guardian/policy_graph.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace guardian {

// ============================================================================
// Helper: enum ↔ string conversions
// ============================================================================

static std::string risk_level_to_string(RiskLevel level) {
  switch (level) {
  case RiskLevel::LOW:
    return "low";
  case RiskLevel::MEDIUM:
    return "medium";
  case RiskLevel::HIGH:
    return "high";
  case RiskLevel::CRITICAL:
    return "critical";
  default:
    return "low";
  }
}

static RiskLevel string_to_risk_level(const std::string &s) {
  if (s == "medium")
    return RiskLevel::MEDIUM;
  if (s == "high")
    return RiskLevel::HIGH;
  if (s == "critical")
    return RiskLevel::CRITICAL;
  return RiskLevel::LOW;
}

static std::string node_type_to_string(NodeType type) {
  switch (type) {
  case NodeType::NORMAL:
    return "normal";
  case NodeType::SENSITIVE_SOURCE:
    return "sensitive_source";
  case NodeType::EXTERNAL_DESTINATION:
    return "external_destination";
  case NodeType::DATA_PROCESSOR:
    return "data_processor";
  default:
    return "normal";
  }
}

static NodeType string_to_node_type(const std::string &s) {
  if (s == "sensitive_source")
    return NodeType::SENSITIVE_SOURCE;
  if (s == "external_destination")
    return NodeType::EXTERNAL_DESTINATION;
  if (s == "data_processor")
    return NodeType::DATA_PROCESSOR;
  return NodeType::NORMAL;
}

// ============================================================================
// Node Operations
// ============================================================================

void PolicyGraph::add_node(const PolicyNode &node) {
  if (node.id.empty()) {
    throw std::invalid_argument(
        "PolicyGraph::add_node: node id cannot be empty");
  }
  if (nodes_.count(node.id)) {
    throw std::invalid_argument("PolicyGraph::add_node: node '" + node.id +
                                "' already exists");
  }
  nodes_[node.id] = node;
  if (!adjacency_list_.count(node.id)) {
    adjacency_list_[node.id] = {};
  }
  // Phase 3: maintain tool_name → id index
  if (!node.tool_name.empty()) {
    tool_name_to_id_[node.tool_name] = node.id;
  }
}

void PolicyGraph::remove_node(const std::string &node_id) {
  if (!nodes_.count(node_id)) {
    throw std::invalid_argument("PolicyGraph::remove_node: node '" + node_id +
                                "' does not exist");
  }
  // Phase 3: clean tool_name index
  auto &removed_node = nodes_.at(node_id);
  if (!removed_node.tool_name.empty()) {
    tool_name_to_id_.erase(removed_node.tool_name);
  }
  adjacency_list_.erase(node_id);
  for (auto &[src_id, edges] : adjacency_list_) {
    edges.erase(std::remove_if(edges.begin(), edges.end(),
                               [&node_id](const PolicyEdge &e) {
                                 return e.to_node_id == node_id;
                               }),
                edges.end());
  }
  nodes_.erase(node_id);
  edge_cache_.clear(); // invalidate cache after structural change
}

bool PolicyGraph::has_node(const std::string &node_id) const {
  return nodes_.count(node_id) > 0;
}

std::optional<PolicyNode>
PolicyGraph::get_node(const std::string &node_id) const {
  auto it = nodes_.find(node_id);
  if (it == nodes_.end()) {
    return std::nullopt;
  }
  return it->second;
}

// ============================================================================
// Edge Operations
// ============================================================================

void PolicyGraph::add_edge(const PolicyEdge &edge) {
  if (edge.from_node_id.empty() || edge.to_node_id.empty()) {
    throw std::invalid_argument(
        "PolicyGraph::add_edge: from_node_id and to_node_id cannot be empty");
  }
  if (!nodes_.count(edge.from_node_id)) {
    throw std::invalid_argument("PolicyGraph::add_edge: source node '" +
                                edge.from_node_id + "' does not exist");
  }
  if (!nodes_.count(edge.to_node_id)) {
    throw std::invalid_argument("PolicyGraph::add_edge: destination node '" +
                                edge.to_node_id + "' does not exist");
  }
  auto &edges = adjacency_list_[edge.from_node_id];
  for (const auto &existing : edges) {
    if (existing.to_node_id == edge.to_node_id) {
      throw std::invalid_argument("PolicyGraph::add_edge: edge from '" +
                                  edge.from_node_id + "' to '" +
                                  edge.to_node_id + "' already exists");
    }
  }
  edges.push_back(edge);
}

void PolicyGraph::remove_edge(const std::string &from_id,
                              const std::string &to_id) {
  auto it = adjacency_list_.find(from_id);
  if (it == adjacency_list_.end()) {
    throw std::invalid_argument("PolicyGraph::remove_edge: source node '" +
                                from_id + "' not found");
  }
  auto &edges = it->second;
  auto original_size = edges.size();
  edges.erase(std::remove_if(edges.begin(), edges.end(),
                             [&to_id](const PolicyEdge &e) {
                               return e.to_node_id == to_id;
                             }),
              edges.end());
  if (edges.size() == original_size) {
    throw std::invalid_argument("PolicyGraph::remove_edge: edge from '" +
                                from_id + "' to '" + to_id +
                                "' does not exist");
  }
}

bool PolicyGraph::has_edge(const std::string &from_id,
                           const std::string &to_id) const {
  auto it = adjacency_list_.find(from_id);
  if (it == adjacency_list_.end()) {
    return false;
  }
  const auto &edges = it->second;
  return std::any_of(edges.begin(), edges.end(), [&to_id](const PolicyEdge &e) {
    return e.to_node_id == to_id;
  });
}

std::vector<PolicyEdge>
PolicyGraph::get_neighbors(const std::string &node_id) const {
  auto it = adjacency_list_.find(node_id);
  if (it == adjacency_list_.end()) {
    return {};
  }
  return it->second;
}

// ============================================================================
// Accessors
// ============================================================================

size_t PolicyGraph::node_count() const { return nodes_.size(); }

size_t PolicyGraph::edge_count() const {
  size_t count = 0;
  for (const auto &[node_id, edges] : adjacency_list_) {
    count += edges.size();
  }
  return count;
}

std::vector<std::string> PolicyGraph::get_all_node_ids() const {
  std::vector<std::string> ids;
  ids.reserve(nodes_.size());
  for (const auto &[id, node] : nodes_) {
    ids.push_back(id);
  }
  return ids;
}

// ============================================================================
// JSON Serialization
// ============================================================================

static json sandbox_config_to_json(const SandboxConfig &cfg) {
  return json{{"memory_limit_mb", cfg.memory_limit_mb},
              {"timeout_ms", cfg.timeout_ms},
              {"allowed_paths", cfg.allowed_paths},
              {"network_access", cfg.network_access},
              {"environment_vars", cfg.environment_vars}};
}

static SandboxConfig json_to_sandbox_config(const json &j) {
  SandboxConfig cfg;
  if (j.contains("memory_limit_mb"))
    cfg.memory_limit_mb = j["memory_limit_mb"].get<uint64_t>();
  if (j.contains("timeout_ms"))
    cfg.timeout_ms = j["timeout_ms"].get<uint32_t>();
  if (j.contains("allowed_paths"))
    cfg.allowed_paths = j["allowed_paths"].get<std::vector<std::string>>();
  if (j.contains("network_access"))
    cfg.network_access = j["network_access"].get<bool>();
  if (j.contains("environment_vars"))
    cfg.environment_vars =
        j["environment_vars"].get<std::map<std::string, std::string>>();
  return cfg;
}

std::string PolicyGraph::to_json() const {
  json j;

  // Serialize nodes
  json nodes_array = json::array();
  for (const auto &[id, node] : nodes_) {
    json node_obj = {
        {"id", node.id},
        {"tool_name", node.tool_name},
        {"risk_level", risk_level_to_string(node.risk_level)},
        {"node_type", node_type_to_string(node.node_type)},
        {"wasm_module", node.wasm_module},
        {"sandbox_config", sandbox_config_to_json(node.sandbox_config)}};
    if (!node.metadata.empty()) {
      node_obj["metadata"] = node.metadata;
    }
    nodes_array.push_back(node_obj);
  }
  j["nodes"] = nodes_array;

  // Serialize edges
  json edges_array = json::array();
  for (const auto &[from_id, edges] : adjacency_list_) {
    for (const auto &edge : edges) {
      json edge_obj = {{"from", edge.from_node_id}, {"to", edge.to_node_id}};
      if (!edge.conditions.empty()) {
        edge_obj["conditions"] = edge.conditions;
      }
      if (!edge.metadata.empty()) {
        edge_obj["metadata"] = edge.metadata;
      }
      edges_array.push_back(edge_obj);
    }
  }
  j["edges"] = edges_array;

  return j.dump(2);
}

PolicyGraph PolicyGraph::from_json(const std::string &json_str) {
  json j;
  try {
    j = json::parse(json_str);
  } catch (const json::parse_error &e) {
    throw std::invalid_argument(
        std::string("PolicyGraph::from_json: invalid JSON — ") + e.what());
  }

  PolicyGraph graph;

  // Deserialize nodes
  if (!j.contains("nodes") || !j["nodes"].is_array()) {
    throw std::invalid_argument(
        "PolicyGraph::from_json: 'nodes' array is required");
  }

  for (const auto &node_obj : j["nodes"]) {
    PolicyNode node;
    if (!node_obj.contains("id") || !node_obj["id"].is_string()) {
      throw std::invalid_argument(
          "PolicyGraph::from_json: each node must have a string 'id'");
    }
    node.id = node_obj["id"].get<std::string>();
    if (node_obj.contains("tool_name"))
      node.tool_name = node_obj["tool_name"].get<std::string>();
    if (node_obj.contains("risk_level"))
      node.risk_level =
          string_to_risk_level(node_obj["risk_level"].get<std::string>());
    if (node_obj.contains("node_type"))
      node.node_type =
          string_to_node_type(node_obj["node_type"].get<std::string>());
    if (node_obj.contains("wasm_module"))
      node.wasm_module = node_obj["wasm_module"].get<std::string>();
    if (node_obj.contains("metadata"))
      node.metadata =
          node_obj["metadata"].get<std::map<std::string, std::string>>();
    if (node_obj.contains("sandbox_config")) {
      node.sandbox_config = json_to_sandbox_config(node_obj["sandbox_config"]);
    }
    graph.add_node(node);
  }

  // Deserialize edges
  if (j.contains("edges") && j["edges"].is_array()) {
    for (const auto &edge_obj : j["edges"]) {
      PolicyEdge edge;
      if (!edge_obj.contains("from") || !edge_obj.contains("to")) {
        throw std::invalid_argument(
            "PolicyGraph::from_json: each edge must have 'from' and 'to'");
      }
      edge.from_node_id = edge_obj["from"].get<std::string>();
      edge.to_node_id = edge_obj["to"].get<std::string>();
      if (edge_obj.contains("conditions")) {
        edge.conditions =
            edge_obj["conditions"].get<std::map<std::string, std::string>>();
      }
      if (edge_obj.contains("metadata")) {
        edge.metadata =
            edge_obj["metadata"].get<std::map<std::string, std::string>>();
      }
      graph.add_edge(edge);
    }
  }

  return graph;
}

// ============================================================================
// DOT Serialization
// ============================================================================

static std::string dot_color_for_node_type(NodeType type) {
  switch (type) {
  case NodeType::SENSITIVE_SOURCE:
    return "red";
  case NodeType::EXTERNAL_DESTINATION:
    return "orange";
  case NodeType::DATA_PROCESSOR:
    return "green";
  default:
    return "lightblue";
  }
}

static std::string dot_shape_for_risk(RiskLevel level) {
  switch (level) {
  case RiskLevel::CRITICAL:
    return "doubleoctagon";
  case RiskLevel::HIGH:
    return "octagon";
  case RiskLevel::MEDIUM:
    return "hexagon";
  default:
    return "box";
  }
}

std::string PolicyGraph::to_dot() const {
  std::ostringstream oss;
  oss << "digraph PolicyGraph {\n";
  oss << "    rankdir=LR;\n";
  oss << "    node [style=filled, fontname=\"Arial\"];\n\n";

  // Nodes
  for (const auto &[id, node] : nodes_) {
    oss << "    \"" << id << "\" ["
        << "label=\"" << node.tool_name << "\\n("
        << node_type_to_string(node.node_type) << ")\""
        << ", fillcolor=\"" << dot_color_for_node_type(node.node_type) << "\""
        << ", shape=\"" << dot_shape_for_risk(node.risk_level) << "\""
        << "];\n";
  }
  oss << "\n";

  // Edges
  for (const auto &[from_id, edges] : adjacency_list_) {
    for (const auto &edge : edges) {
      oss << "    \"" << edge.from_node_id << "\" -> \"" << edge.to_node_id
          << "\"";
      if (!edge.conditions.empty()) {
        std::string label;
        for (const auto &[k, v] : edge.conditions) {
          if (!label.empty())
            label += ", ";
          label += k + "=" + v;
        }
        oss << " [label=\"" << label << "\"]";
      }
      oss << ";\n";
    }
  }

  oss << "}\n";
  return oss.str();
}

PolicyGraph PolicyGraph::from_dot(const std::string &dot_str) {
  PolicyGraph graph;

  // Simple DOT parser — handles node definitions and edges
  std::istringstream stream(dot_str);
  std::string line;

  // Regex patterns for DOT parsing
  std::regex node_regex(R"re(\s*"([^"]+)"\s*\[(.+)\]\s*;)re");
  std::regex edge_regex(R"re(\s*"([^"]+)"\s*->\s*"([^"]+)")re");
  std::regex label_regex(R"re(label="([^"]*)")re");
  std::regex fill_regex(R"re(fillcolor="([^"]*)")re");
  std::regex shape_regex(R"re(shape="([^"]*)")re");

  // First pass: collect nodes
  while (std::getline(stream, line)) {
    std::smatch match;
    if (std::regex_search(line, match, node_regex)) {
      PolicyNode node;
      node.id = match[1].str();
      std::string attrs = match[2].str();

      // Extract label → tool_name
      std::smatch label_match;
      if (std::regex_search(attrs, label_match, label_regex)) {
        std::string label = label_match[1].str();
        auto nl_pos = label.find("\\n");
        node.tool_name =
            (nl_pos != std::string::npos) ? label.substr(0, nl_pos) : label;

        // Extract node_type from label parenthetical
        if (nl_pos != std::string::npos) {
          auto type_str = label.substr(nl_pos + 2);
          // Remove parentheses
          if (type_str.front() == '(')
            type_str = type_str.substr(1);
          if (type_str.back() == ')')
            type_str.pop_back();
          node.node_type = string_to_node_type(type_str);
        }
      }

      // Extract fillcolor → node_type (fallback)
      std::smatch fill_match;
      if (std::regex_search(attrs, fill_match, fill_regex)) {
        std::string color = fill_match[1].str();
        if (color == "red")
          node.node_type = NodeType::SENSITIVE_SOURCE;
        else if (color == "orange")
          node.node_type = NodeType::EXTERNAL_DESTINATION;
        else if (color == "green")
          node.node_type = NodeType::DATA_PROCESSOR;
      }

      // Extract shape → risk_level
      std::smatch shape_match;
      if (std::regex_search(attrs, shape_match, shape_regex)) {
        std::string shape = shape_match[1].str();
        if (shape == "doubleoctagon")
          node.risk_level = RiskLevel::CRITICAL;
        else if (shape == "octagon")
          node.risk_level = RiskLevel::HIGH;
        else if (shape == "hexagon")
          node.risk_level = RiskLevel::MEDIUM;
      }

      graph.add_node(node);
    }
  }

  // Second pass: collect edges
  stream.clear();
  stream.str(dot_str);
  while (std::getline(stream, line)) {
    std::smatch match;
    if (std::regex_search(line, match, edge_regex)) {
      PolicyEdge edge;
      edge.from_node_id = match[1].str();
      edge.to_node_id = match[2].str();

      // Extract edge label as conditions
      std::smatch label_match;
      if (std::regex_search(line, label_match, label_regex)) {
        std::string label_str = label_match[1].str();
        // Parse "key=value, key=value"
        std::istringstream pairs(label_str);
        std::string pair;
        while (std::getline(pairs, pair, ',')) {
          auto eq = pair.find('=');
          if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string val = pair.substr(eq + 1);
            // Trim whitespace
            key.erase(0, key.find_first_not_of(' '));
            key.erase(key.find_last_not_of(' ') + 1);
            val.erase(0, val.find_first_not_of(' '));
            val.erase(val.find_last_not_of(' ') + 1);
            edge.conditions[key] = val;
          }
        }
      }

      // Only add if both nodes exist
      if (graph.has_node(edge.from_node_id) &&
          graph.has_node(edge.to_node_id)) {
        graph.add_edge(edge);
      }
    }
  }

  return graph;
}

// ============================================================================
// Phase 3: StringPool Implementation
// ============================================================================

const std::string &StringPool::intern(const std::string &s) {
  auto [it, _] = pool_.insert(s);
  return *it;
}

// ============================================================================
// Phase 3: Performance Features
// ============================================================================

void PolicyGraph::clear_caches() {
  edge_cache_.clear();
  string_pool_.clear();
}

std::optional<PolicyNode>
PolicyGraph::get_node_by_tool_name(const std::string &tool_name) const {
  auto it = tool_name_to_id_.find(tool_name);
  if (it == tool_name_to_id_.end()) {
    return std::nullopt;
  }
  return get_node(it->second);
}

// BFS-based path finding
std::vector<std::string>
PolicyGraph::find_path(const std::string &from_id,
                       const std::string &to_id) const {
  if (!has_node(from_id) || !has_node(to_id))
    return {};
  if (from_id == to_id)
    return {from_id};

  std::unordered_map<std::string, std::string> parent;
  std::vector<std::string> queue;
  std::unordered_set<std::string> visited;

  queue.push_back(from_id);
  visited.insert(from_id);
  parent[from_id] = "";

  size_t front = 0;
  while (front < queue.size()) {
    auto current = queue[front++];
    auto neighbors = get_neighbors(current);
    for (const auto &edge : neighbors) {
      if (visited.count(edge.to_node_id))
        continue;
      parent[edge.to_node_id] = current;
      if (edge.to_node_id == to_id) {
        // Reconstruct path
        std::vector<std::string> path;
        std::string node = to_id;
        while (!node.empty()) {
          path.push_back(node);
          node = parent[node];
        }
        std::reverse(path.begin(), path.end());
        return path;
      }
      visited.insert(edge.to_node_id);
      queue.push_back(edge.to_node_id);
    }
  }
  return {}; // no path found
}

bool PolicyGraph::is_reachable(const std::string &from_id,
                               const std::string &to_id) const {
  // Check cache first
  std::string cache_key = from_id + "->" + to_id;
  // Note: edge_cache_ is mutable-like via const_cast for perf caching
  // In production, use mutable keyword. For now, do BFS directly.
  if (!has_node(from_id) || !has_node(to_id))
    return false;
  if (from_id == to_id)
    return true;
  return !find_path(from_id, to_id).empty();
}

} // namespace guardian
