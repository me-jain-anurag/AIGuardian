#include <fstream>
// examples/basic_integration.cpp
// Owner: Dev A
// Shows basic Guardian initialization, session management, and tool validation
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>

#include "guardian/guardian.hpp"
#include "guardian/policy_graph.hpp"

using namespace guardian;
using std::cout;
using std::endl;

// Helper: create a sample policy file
static std::string create_sample_policy() {
    PolicyGraph graph;

    PolicyNode n1;
    n1.id = "read_accounts";
    n1.tool_name = "read_accounts";
    n1.node_type = NodeType::SENSITIVE_SOURCE;
    n1.risk_level = RiskLevel::HIGH;
    graph.add_node(n1);

    PolicyNode n2;
    n2.id = "generate_report";
    n2.tool_name = "generate_report";
    n2.node_type = NodeType::DATA_PROCESSOR;
    n2.risk_level = RiskLevel::MEDIUM;
    graph.add_node(n2);

    PolicyNode n3;
    n3.id = "send_email";
    n3.tool_name = "send_email";
    n3.node_type = NodeType::EXTERNAL_DESTINATION;
    n3.risk_level = RiskLevel::HIGH;
    graph.add_node(n3);

    PolicyEdge e1;
    e1.from_node_id = "read_accounts";
    e1.to_node_id = "generate_report";
    graph.add_edge(e1);

    PolicyEdge e2;
    e2.from_node_id = "generate_report";
    e2.to_node_id = "send_email";
    graph.add_edge(e2);

    auto path = std::filesystem::temp_directory_path() / "example_policy.json";
    std::ofstream file(path);
    file << graph.to_json();
    file.close();

    return path.string();
}

int main() {
    cout << "=== Guardian AI — Basic Integration Example ===" << endl << endl;

    try {
        // Step 1: Create a policy and initialize Guardian
        auto policy_path = create_sample_policy();
        cout << "1. Created sample policy at: " << policy_path << endl;

        Guardian guardian(policy_path);
        cout << "2. Guardian initialized successfully!" << endl;
        cout << "   Policy has " << guardian.get_policy_graph().node_count()
             << " nodes and " << guardian.get_policy_graph().edge_count()
             << " edges" << endl << endl;

        // Step 2: Create a session
        auto session_id = guardian.create_session();
        cout << "3. Created session: " << session_id << endl << endl;

        // Step 3: Validate tool calls
        cout << "4. Validating tool calls:" << endl;

        auto r1 = guardian.validate_tool_call("read_accounts", session_id);
        cout << "   read_accounts → "
             << (r1.approved ? "APPROVED" : "BLOCKED") << endl;

        auto r2 = guardian.validate_tool_call("generate_report", session_id);
        cout << "   generate_report → "
             << (r2.approved ? "APPROVED" : "BLOCKED") << endl;

        auto r3 = guardian.validate_tool_call("unknown_tool", session_id);
        cout << "   unknown_tool → "
             << (r3.approved ? "APPROVED" : "BLOCKED")
             << " (" << r3.reason << ")" << endl << endl;

        // Step 4: Execute a tool
        cout << "5. Executing tool with sandbox:" << endl;
        auto [validation, sandbox] = guardian.execute_tool(
            "read_accounts", {{"account_id", "12345"}}, session_id);
        cout << "   Validation: " << (validation.approved ? "APPROVED" : "BLOCKED") << endl;
        cout << "   Sandbox: " << (sandbox.has_value() ? "executed" : "not active") << endl << endl;

        // Step 5: Visualize the policy
        cout << "6. Policy graph (DOT format):" << endl;
        cout << guardian.visualize_policy("dot") << endl;

        // Step 6: End session
        guardian.end_session(session_id);
        cout << "7. Session ended." << endl;

        // Cleanup
        std::filesystem::remove(policy_path);
        cout << endl << "=== Example complete ===" << endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
