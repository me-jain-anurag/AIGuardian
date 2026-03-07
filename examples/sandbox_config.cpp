// examples/sandbox_config.cpp
// Owner: Dev A
// Demonstrates how to configure and override Sandbox limits
#include "guardian/guardian.hpp"
#include <iostream>

using namespace guardian;

int main() {
    try {
        // Create an empty policy for this demo
        PolicyGraph graph;
        
        PolicyNode n1;
        n1.id = "heavy_compute";
        n1.tool_name = "heavy_compute";
        // Node-specific sandbox override (overrides default config)
        n1.sandbox_config.memory_limit_mb = 1024; // Needs 1GB
        n1.sandbox_config.timeout_ms = 30000;     // Needs 30s
        graph.add_node(n1);

        PolicyNode n2;
        n2.id = "quick_network";
        n2.tool_name = "quick_network";
        n2.sandbox_config.network_access = true;
        n2.sandbox_config.timeout_ms = 1000;      // Fails if > 1s
        graph.add_node(n2);

        // Save layout
        std::string policy_file = "sandbox_demo_policy.json";
        std::ofstream out(policy_file);
        out << graph.to_json();
        out.close();

        // 1. Initialize API
        Guardian g(policy_file);

        // 2. Set global default sandbox config (applies to nodes that don't override)
        SandboxConfig global_sandbox;
        global_sandbox.memory_limit_mb = 128; // Strict global default
        global_sandbox.timeout_ms = 5000;     // 5s default
        global_sandbox.network_access = false; // Block network by default
        
        Config app_config;
        app_config.sandbox = global_sandbox;
        g.update_config(app_config);

        std::cout << "Global default memory limit: " 
                  << g.get_config().sandbox.memory_limit_mb << " MB\n";

        // 3. Inspect specific nodes to see overrides
        auto node1 = g.get_policy_graph().get_node("heavy_compute");
        if (node1) {
            std::cout << "Node 'heavy_compute' memory limit override: " 
                      << node1->sandbox_config.memory_limit_mb << " MB\n";
        }

        auto node2 = g.get_policy_graph().get_node("quick_network");
        if (node2) {
            std::cout << "Node 'quick_network' network access: " 
                      << (node2->sandbox_config.network_access ? "ALLOWED" : "DENIED") << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
