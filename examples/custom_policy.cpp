// examples/custom_policy.cpp
// Owner: Dev A
// Shows programmatic policy graph creation, serialization, and querying
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

#include "guardian/policy_graph.hpp"

using namespace guardian;
using std::cout;
using std::endl;

int main() {
    cout << "=== Guardian AI — Custom Policy Example ===" << endl << endl;

    try {
        // Step 1: Build a policy graph programmatically
        PolicyGraph graph;

        // Add nodes with different types
        PolicyNode read_code;
        read_code.id = "read_code";
        read_code.tool_name = "read_code";
        read_code.node_type = NodeType::SENSITIVE_SOURCE;
        read_code.risk_level = RiskLevel::HIGH;
        read_code.metadata["description"] = "Reads source code files";
        graph.add_node(read_code);

        PolicyNode run_tests;
        run_tests.id = "run_tests";
        run_tests.tool_name = "run_tests";
        run_tests.node_type = NodeType::DATA_PROCESSOR;
        run_tests.risk_level = RiskLevel::LOW;
        graph.add_node(run_tests);

        PolicyNode approval;
        approval.id = "approval_request";
        approval.tool_name = "approval_request";
        approval.node_type = NodeType::DATA_PROCESSOR;
        approval.risk_level = RiskLevel::MEDIUM;
        graph.add_node(approval);

        PolicyNode deploy;
        deploy.id = "deploy_production";
        deploy.tool_name = "deploy_production";
        deploy.node_type = NodeType::EXTERNAL_DESTINATION;
        deploy.risk_level = RiskLevel::CRITICAL;
        deploy.sandbox_config.memory_limit_mb = 256;
        deploy.sandbox_config.timeout_ms = 30000;
        deploy.sandbox_config.network_access = true;
        graph.add_node(deploy);

        // Add edges defining allowed transitions
        PolicyEdge e1;
        e1.from_node_id = "read_code";
        e1.to_node_id = "run_tests";
        e1.conditions["requires"] = "test_files_exist";
        graph.add_edge(e1);

        PolicyEdge e2;
        e2.from_node_id = "read_code";
        e2.to_node_id = "approval_request";
        graph.add_edge(e2);

        PolicyEdge e3;
        e3.from_node_id = "approval_request";
        e3.to_node_id = "deploy_production";
        e3.conditions["requires"] = "manager_approval";
        graph.add_edge(e3);

        // NOTE: No direct edge from read_code → deploy_production
        // This blocks unauthorized deployments!

        cout << "1. Built policy graph with " << graph.node_count()
             << " nodes and " << graph.edge_count() << " edges" << endl << endl;

        // Step 2: Query the graph
        cout << "2. Graph queries:" << endl;
        cout << "   has read_code? " << (graph.has_node("read_code") ? "yes" : "no") << endl;
        cout << "   read_code → run_tests? " << (graph.has_edge("read_code", "run_tests") ? "yes" : "no") << endl;
        cout << "   read_code → deploy? " << (graph.has_edge("read_code", "deploy_production") ? "ALLOWED" : "BLOCKED") << endl;
        cout << "   approval → deploy? " << (graph.has_edge("approval_request", "deploy_production") ? "ALLOWED" : "BLOCKED") << endl;

        auto neighbors = graph.get_neighbors("read_code");
        cout << "   read_code neighbors: ";
        for (const auto& e : neighbors) cout << e.to_node_id << " ";
        cout << endl << endl;

        // Step 3: Serialize to JSON
        std::string json_str = graph.to_json();
        cout << "3. JSON serialization:" << endl;
        cout << json_str << endl << endl;

        // Step 4: Save to file
        auto output_path = std::filesystem::temp_directory_path() / "developer_policy.json";
        std::ofstream file(output_path);
        file << json_str;
        file.close();
        cout << "4. Saved to " << output_path << endl << endl;

        // Step 5: Load it back
        auto loaded = PolicyGraph::from_json(json_str);
        cout << "5. Loaded back: " << loaded.node_count() << " nodes, "
             << loaded.edge_count() << " edges" << endl << endl;

        // Step 6: DOT format
        cout << "6. DOT format (for Graphviz):" << endl;
        cout << graph.to_dot() << endl;

        // Cleanup
        std::filesystem::remove(output_path);
        cout << "=== Example complete ===" << endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
