// examples/concurrent_sessions.cpp
// Owner: Dev C

#include "guardian/policy_graph.hpp"
#include "guardian/policy_validator.hpp"
#include "guardian/sandbox_manager.hpp"
#include "guardian/session_manager.hpp"
#include "guardian/tool_interceptor.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace guardian;

void run_session(ToolInterceptor &interceptor, SessionManager &session_mgr,
                 const std::string &session_name, int iterations) {
  std::string sid = session_mgr.create_session();
  std::cout << "Started session: " << session_name << " (ID: " << sid << ")\n";

  for (int i = 0; i < iterations; ++i) {
    auto [val, sb] =
        interceptor.intercept(sid, "safe_tool", {{"iter", std::to_string(i)}});
    if (!val.approved) {
      std::cout << session_name << " blocked: " << val.reason << "\n";
      break;
    }

    // Demonstrate persistence with per-session audit files.
    std::string path = "sessions/" + session_name + "_" + sid + ".json";
    session_mgr.persist_session(sid, path);
    std::cout << "Saved session " << session_name << " to " << path << "\n";
  }

  std::cout << "Finished session: " << session_name << "\n";
}

int main() {
  PolicyGraph graph;
  graph.add_node({"safe_tool", "safe_tool", RiskLevel::LOW, NodeType::NORMAL,
                  {}, "", {}});
  graph.add_edge({"safe_tool", "safe_tool", {}, {}});

  PolicyValidator validator(graph);
  validator.set_cycle_threshold(100);

  SessionManager session_mgr;
  auto mock_factory = [](const std::string &, const SandboxConfig &) {
    return std::make_unique<MockRuntime>();
  };
  SandboxManager sandbox_mgr("", mock_factory);
  sandbox_mgr.load_module("safe_tool", "safe_tool.wasm");

  ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);

  std::vector<std::thread> threads;
  std::cout << "Launching 3 concurrent agent sessions...\n";
  for (int i = 0; i < 3; ++i) {
    threads.emplace_back(run_session, std::ref(interceptor), std::ref(session_mgr),
                         "Agent_" + std::to_string(i), 6);
  }

  for (auto &t : threads) {
    t.join();
  }

  std::cout << "All sessions complete. The interceptor and session manager "
               "successfully handled concurrency!\n";
  return 0;
}
