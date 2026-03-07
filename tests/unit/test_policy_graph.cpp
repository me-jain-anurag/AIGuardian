// tests/unit/test_policy_graph.cpp
// Owner: Dev A
// Unit tests for PolicyGraph and JSON/DOT serialization
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "guardian/policy_graph.hpp"

using namespace guardian;

// ============================================================================
// Helper: create a simple test graph
// ============================================================================

static PolicyGraph make_test_graph() {
    PolicyGraph graph;

    PolicyNode n1;
    n1.id = "read_accounts";
    n1.tool_name = "read_accounts";
    n1.risk_level = RiskLevel::HIGH;
    n1.node_type = NodeType::SENSITIVE_SOURCE;
    graph.add_node(n1);

    PolicyNode n2;
    n2.id = "generate_report";
    n2.tool_name = "generate_report";
    n2.risk_level = RiskLevel::MEDIUM;
    n2.node_type = NodeType::DATA_PROCESSOR;
    graph.add_node(n2);

    PolicyNode n3;
    n3.id = "send_email";
    n3.tool_name = "send_email";
    n3.risk_level = RiskLevel::HIGH;
    n3.node_type = NodeType::EXTERNAL_DESTINATION;
    graph.add_node(n3);

    PolicyEdge e1;
    e1.from_node_id = "read_accounts";
    e1.to_node_id = "generate_report";
    graph.add_edge(e1);

    PolicyEdge e2;
    e2.from_node_id = "generate_report";
    e2.to_node_id = "send_email";
    graph.add_edge(e2);

    return graph;
}

// ============================================================================
// Empty Graph Tests
// ============================================================================

TEST_CASE("Empty graph has zero nodes and edges", "[policy-graph]") {
    PolicyGraph graph;
    REQUIRE(graph.node_count() == 0);
    REQUIRE(graph.edge_count() == 0);
    REQUIRE(graph.get_all_node_ids().empty());
}

TEST_CASE("Empty graph has_node returns false", "[policy-graph]") {
    PolicyGraph graph;
    REQUIRE_FALSE(graph.has_node("nonexistent"));
}

TEST_CASE("Empty graph get_node returns nullopt", "[policy-graph]") {
    PolicyGraph graph;
    REQUIRE_FALSE(graph.get_node("nonexistent").has_value());
}

// ============================================================================
// Node Addition Tests
// ============================================================================

TEST_CASE("Add single node", "[policy-graph][nodes]") {
    PolicyGraph graph;
    PolicyNode node;
    node.id = "test_node";
    node.tool_name = "test_tool";
    node.risk_level = RiskLevel::LOW;
    node.node_type = NodeType::NORMAL;

    graph.add_node(node);

    REQUIRE(graph.node_count() == 1);
    REQUIRE(graph.has_node("test_node"));

    auto retrieved = graph.get_node("test_node");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->tool_name == "test_tool");
    REQUIRE(retrieved->risk_level == RiskLevel::LOW);
}

TEST_CASE("Add multiple nodes", "[policy-graph][nodes]") {
    auto graph = make_test_graph();
    REQUIRE(graph.node_count() == 3);
    REQUIRE(graph.has_node("read_accounts"));
    REQUIRE(graph.has_node("generate_report"));
    REQUIRE(graph.has_node("send_email"));
}

TEST_CASE("Add node with empty id throws", "[policy-graph][nodes][error]") {
    PolicyGraph graph;
    PolicyNode node;
    node.id = "";
    REQUIRE_THROWS_AS(graph.add_node(node), std::invalid_argument);
}

TEST_CASE("Add duplicate node throws", "[policy-graph][nodes][error]") {
    PolicyGraph graph;
    PolicyNode node;
    node.id = "dup";
    graph.add_node(node);
    REQUIRE_THROWS_AS(graph.add_node(node), std::invalid_argument);
}

// ============================================================================
// Node Removal Tests
// ============================================================================

TEST_CASE("Remove node", "[policy-graph][nodes]") {
    auto graph = make_test_graph();
    graph.remove_node("generate_report");

    REQUIRE(graph.node_count() == 2);
    REQUIRE_FALSE(graph.has_node("generate_report"));
    // Edges to/from removed node should be gone
    REQUIRE_FALSE(graph.has_edge("read_accounts", "generate_report"));
    REQUIRE_FALSE(graph.has_edge("generate_report", "send_email"));
}

TEST_CASE("Remove nonexistent node throws", "[policy-graph][nodes][error]") {
    PolicyGraph graph;
    REQUIRE_THROWS_AS(graph.remove_node("nonexistent"), std::invalid_argument);
}

// ============================================================================
// Edge Addition Tests
// ============================================================================

TEST_CASE("Add edge between existing nodes", "[policy-graph][edges]") {
    auto graph = make_test_graph();
    REQUIRE(graph.edge_count() == 2);
    REQUIRE(graph.has_edge("read_accounts", "generate_report"));
    REQUIRE(graph.has_edge("generate_report", "send_email"));
}

TEST_CASE("Edge to nonexistent source throws", "[policy-graph][edges][error]") {
    PolicyGraph graph;
    PolicyNode n;
    n.id = "a";
    graph.add_node(n);

    PolicyEdge e;
    e.from_node_id = "missing";
    e.to_node_id = "a";
    REQUIRE_THROWS_AS(graph.add_edge(e), std::invalid_argument);
}

TEST_CASE("Edge to nonexistent dest throws", "[policy-graph][edges][error]") {
    PolicyGraph graph;
    PolicyNode n;
    n.id = "a";
    graph.add_node(n);

    PolicyEdge e;
    e.from_node_id = "a";
    e.to_node_id = "missing";
    REQUIRE_THROWS_AS(graph.add_edge(e), std::invalid_argument);
}

TEST_CASE("Duplicate edge throws", "[policy-graph][edges][error]") {
    auto graph = make_test_graph();
    PolicyEdge e;
    e.from_node_id = "read_accounts";
    e.to_node_id = "generate_report";
    REQUIRE_THROWS_AS(graph.add_edge(e), std::invalid_argument);
}

TEST_CASE("Edge with empty id throws", "[policy-graph][edges][error]") {
    PolicyGraph graph;
    PolicyEdge e;
    e.from_node_id = "";
    e.to_node_id = "";
    REQUIRE_THROWS_AS(graph.add_edge(e), std::invalid_argument);
}

// ============================================================================
// Edge Removal Tests
// ============================================================================

TEST_CASE("Remove edge", "[policy-graph][edges]") {
    auto graph = make_test_graph();
    graph.remove_edge("read_accounts", "generate_report");

    REQUIRE(graph.edge_count() == 1);
    REQUIRE_FALSE(graph.has_edge("read_accounts", "generate_report"));
    REQUIRE(graph.has_edge("generate_report", "send_email"));
}

TEST_CASE("Remove nonexistent edge throws", "[policy-graph][edges][error]") {
    auto graph = make_test_graph();
    REQUIRE_THROWS_AS(
        graph.remove_edge("read_accounts", "send_email"),
        std::invalid_argument);
}

// ============================================================================
// get_neighbors Tests
// ============================================================================

TEST_CASE("get_neighbors returns outgoing edges", "[policy-graph][query]") {
    auto graph = make_test_graph();
    auto neighbors = graph.get_neighbors("read_accounts");
    REQUIRE(neighbors.size() == 1);
    REQUIRE(neighbors[0].to_node_id == "generate_report");
}

TEST_CASE("get_neighbors of leaf node returns empty", "[policy-graph][query]") {
    auto graph = make_test_graph();
    auto neighbors = graph.get_neighbors("send_email");
    REQUIRE(neighbors.empty());
}

TEST_CASE("get_neighbors of nonexistent node returns empty", "[policy-graph][query]") {
    PolicyGraph graph;
    REQUIRE(graph.get_neighbors("missing").empty());
}

// ============================================================================
// get_all_node_ids Tests
// ============================================================================

TEST_CASE("get_all_node_ids returns all ids", "[policy-graph][query]") {
    auto graph = make_test_graph();
    auto ids = graph.get_all_node_ids();
    REQUIRE(ids.size() == 3);
    // Check all three are present (order not guaranteed with unordered_map)
    REQUIRE(std::find(ids.begin(), ids.end(), "read_accounts") != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), "generate_report") != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), "send_email") != ids.end());
}

// ============================================================================
// Node Metadata Preservation
// ============================================================================

TEST_CASE("Node metadata is preserved", "[policy-graph][metadata]") {
    PolicyGraph graph;
    PolicyNode node;
    node.id = "meta_test";
    node.tool_name = "meta_tool";
    node.metadata["key1"] = "value1";
    node.metadata["key2"] = "value2";
    graph.add_node(node);

    auto retrieved = graph.get_node("meta_test");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->metadata.size() == 2);
    REQUIRE(retrieved->metadata.at("key1") == "value1");
    REQUIRE(retrieved->metadata.at("key2") == "value2");
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

TEST_CASE("JSON round-trip preserves graph", "[policy-graph][json]") {
    auto original = make_test_graph();
    std::string json_str = original.to_json();

    auto restored = PolicyGraph::from_json(json_str);
    REQUIRE(restored.node_count() == original.node_count());
    REQUIRE(restored.edge_count() == original.edge_count());

    // Check nodes
    REQUIRE(restored.has_node("read_accounts"));
    REQUIRE(restored.has_node("generate_report"));
    REQUIRE(restored.has_node("send_email"));

    // Check node types preserved
    auto n1 = restored.get_node("read_accounts");
    REQUIRE(n1->node_type == NodeType::SENSITIVE_SOURCE);
    REQUIRE(n1->risk_level == RiskLevel::HIGH);

    // Check edges
    REQUIRE(restored.has_edge("read_accounts", "generate_report"));
    REQUIRE(restored.has_edge("generate_report", "send_email"));
    REQUIRE_FALSE(restored.has_edge("read_accounts", "send_email"));
}

TEST_CASE("from_json with invalid JSON throws", "[policy-graph][json][error]") {
    REQUIRE_THROWS_AS(PolicyGraph::from_json("{invalid json}"), std::invalid_argument);
}

TEST_CASE("from_json with missing nodes throws", "[policy-graph][json][error]") {
    REQUIRE_THROWS_AS(PolicyGraph::from_json(R"({"edges": []})"), std::invalid_argument);
}

TEST_CASE("from_json with missing node id throws", "[policy-graph][json][error]") {
    REQUIRE_THROWS_AS(
        PolicyGraph::from_json(R"({"nodes": [{"tool_name": "test"}]})"),
        std::invalid_argument);
}

// ============================================================================
// DOT Serialization Tests
// ============================================================================

TEST_CASE("DOT output contains all nodes", "[policy-graph][dot]") {
    auto graph = make_test_graph();
    std::string dot = graph.to_dot();

    REQUIRE(dot.find("digraph") != std::string::npos);
    REQUIRE(dot.find("read_accounts") != std::string::npos);
    REQUIRE(dot.find("generate_report") != std::string::npos);
    REQUIRE(dot.find("send_email") != std::string::npos);
}

TEST_CASE("DOT output contains edges", "[policy-graph][dot]") {
    auto graph = make_test_graph();
    std::string dot = graph.to_dot();

    REQUIRE(dot.find("->") != std::string::npos);
}

TEST_CASE("DOT output uses correct colors", "[policy-graph][dot]") {
    auto graph = make_test_graph();
    std::string dot = graph.to_dot();

    // SENSITIVE_SOURCE should be red
    REQUIRE(dot.find("red") != std::string::npos);
    // EXTERNAL_DESTINATION should be orange
    REQUIRE(dot.find("orange") != std::string::npos);
    // DATA_PROCESSOR should be green
    REQUIRE(dot.find("green") != std::string::npos);
}

TEST_CASE("DOT round-trip preserves structure", "[policy-graph][dot]") {
    auto original = make_test_graph();
    std::string dot_str = original.to_dot();

    auto restored = PolicyGraph::from_dot(dot_str);
    REQUIRE(restored.node_count() == original.node_count());
    REQUIRE(restored.edge_count() == original.edge_count());
}
