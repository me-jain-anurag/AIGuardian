// tests/integration/test_integration_demos.cpp
// Owner: Dev C

#include "guardian/policy_graph.hpp"
#include "guardian/policy_validator.hpp"
#include "guardian/sandbox_manager.hpp"
#include "guardian/session_manager.hpp"
#include "guardian/tool_interceptor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace guardian;
namespace fs = std::filesystem;

void assert_test(bool condition, const std::string &msg) {
  if (!condition) {
    std::cerr << "TEST FAILED: " << msg << std::endl;
    exit(1);
  }
}

void test_financial_scenario() {
  PolicyGraph graph;
  graph.add_node({"read_db", "read_db", RiskLevel::HIGH,
                  NodeType::SENSITIVE_SOURCE, {}, "", {}});
  graph.add_node({"send_email", "send_email", RiskLevel::HIGH,
                  NodeType::EXTERNAL_DESTINATION, {}, "", {}});
  graph.add_node({"process_data", "process_data", RiskLevel::LOW,
                  NodeType::DATA_PROCESSOR, {}, "", {}});
  graph.add_node({"encrypt", "encrypt", RiskLevel::LOW,
                  NodeType::DATA_PROCESSOR, {}, "", {}});

  graph.add_edge({"read_db", "process_data", {}, {}});
  graph.add_edge({"process_data", "encrypt", {}, {}});
  graph.add_edge({"encrypt", "send_email", {}, {}});
  graph.add_edge({"read_db", "send_email", {}, {}});

  PolicyValidator validator(graph);
  SessionManager session_mgr;
  auto mock_factory = [](const std::string &, const SandboxConfig &) {
    return std::make_unique<MockRuntime>();
  };
  SandboxManager sandbox_mgr("", mock_factory);
  sandbox_mgr.load_module("read_db", "read_db.wasm");
  sandbox_mgr.load_module("process_data", "process_data.wasm");
  sandbox_mgr.load_module("encrypt", "encrypt.wasm");
  sandbox_mgr.load_module("send_email", "send_email.wasm");

  ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);

  std::string sid1 = session_mgr.create_session();
  auto [val1, sb1] = interceptor.intercept(sid1, "read_db", {});
  assert_test(val1.approved && sb1.has_value() && sb1->success,
              "read_db should execute successfully");

  auto [val2, sb2] = interceptor.intercept(sid1, "send_email", {});
  assert_test(!val2.approved, "Direct exfiltration should be blocked");
  assert_test(!sb2.has_value(), "Rejected transition should not execute sandbox");
  assert_test(val2.reason.find("Exfiltration") != std::string::npos,
              "Reason should mention Exfiltration");

  std::string sid2 = session_mgr.create_session();
  assert_test(interceptor.intercept(sid2, "read_db", {}).first.approved,
              "Step 1 OK");
  assert_test(interceptor.intercept(sid2, "process_data", {}).first.approved,
              "Step 2 OK");
  assert_test(interceptor.intercept(sid2, "encrypt", {}).first.approved,
              "Step 3 OK");
  assert_test(interceptor.intercept(sid2, "send_email", {}).first.approved,
              "Step 4 OK");

  std::cout << "Integration Scenario 1 (Financial) - Passed!\n";
}

void test_dos_scenario() {
  PolicyGraph graph;
  graph.add_node({"fetch_data", "fetch_data", RiskLevel::LOW, NodeType::NORMAL,
                  {}, "", {}});
  graph.add_edge({"fetch_data", "fetch_data", {}, {}});

  PolicyValidator validator(graph);
  validator.set_cycle_threshold(3);

  SessionManager session_mgr;
  auto mock_factory = [](const std::string &, const SandboxConfig &) {
    return std::make_unique<MockRuntime>();
  };
  SandboxManager sandbox_mgr("", mock_factory);
  sandbox_mgr.load_module("fetch_data", "fetch_data.wasm");

  ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);
  std::string sid = session_mgr.create_session();

  for (int i = 0; i < 3; ++i) {
    auto [val, sb] = interceptor.intercept(sid, "fetch_data", {});
    assert_test(val.approved && sb.has_value() && sb->success,
                "fetch_data should be allowed up to threshold");
  }

  auto [val_block, sb_block] = interceptor.intercept(sid, "fetch_data", {});
  assert_test(!val_block.approved, "fetch_data should be blocked on 4th call");
  assert_test(!sb_block.has_value(), "blocked calls must not execute");
  assert_test(val_block.reason.find("Cycle detected") != std::string::npos,
              "Reason should mention cycle threshold");

  std::cout << "Integration Scenario 2 (DoS) - Passed!\n";
}

void test_developer_scenario() {
  PolicyGraph graph;
  graph.add_node({"read_code", "read_code", RiskLevel::LOW, NodeType::NORMAL,
                  {}, "", {}});
  graph.add_node({"approval_request", "approval_request", RiskLevel::MEDIUM,
                  NodeType::NORMAL, {}, "", {}});
  graph.add_node({"deploy_production", "deploy_production", RiskLevel::HIGH,
                  NodeType::NORMAL, {}, "", {}});

  graph.add_edge({"read_code", "approval_request", {}, {}});
  graph.add_edge({"approval_request", "deploy_production", {}, {}});

  PolicyValidator validator(graph);
  SessionManager session_mgr;
  auto mock_factory = [](const std::string &, const SandboxConfig &) {
    return std::make_unique<MockRuntime>();
  };
  SandboxManager sandbox_mgr("", mock_factory);
  sandbox_mgr.load_module("read_code", "read_code.wasm");
  sandbox_mgr.load_module("approval_request", "approval_request.wasm");
  sandbox_mgr.load_module("deploy_production", "deploy_production.wasm");

  ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);

  std::string sid1 = session_mgr.create_session();
  interceptor.intercept(sid1, "read_code", {});
  auto [val_fail, sb_fail] = interceptor.intercept(sid1, "deploy_production", {});
  assert_test(!val_fail.approved,
              "Direct deploy without approval should be blocked");
  assert_test(!sb_fail.has_value(), "blocked call should not execute");

  std::string sid2 = session_mgr.create_session();
  assert_test(interceptor.intercept(sid2, "read_code", {}).first.approved,
              "Reading code OK");
  assert_test(interceptor.intercept(sid2, "approval_request", {}).first.approved,
              "Requesting approval OK");
  assert_test(interceptor.intercept(sid2, "deploy_production", {}).first.approved,
              "Deploying to production OK");

  std::cout << "Integration Scenario 3 (Developer) - Passed!\n";
}

void test_concurrent_sessions() {
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
  std::string sid_a = session_mgr.create_session();
  std::string sid_b = session_mgr.create_session();

  auto worker = [&](const std::string &sid, int n) {
    for (int i = 0; i < n; ++i) {
      auto [val, sb] = interceptor.intercept(
          sid, "safe_tool", {{"idx", std::to_string(i)}});
      assert_test(val.approved, "Concurrent calls should be approved");
      assert_test(sb.has_value() && sb->success,
                  "Concurrent calls should execute successfully");
    }
  };

  std::thread t1(worker, sid_a, 8);
  std::thread t2(worker, sid_b, 5);
  t1.join();
  t2.join();

  assert_test(session_mgr.get_sequence(sid_a).size() == 8,
              "Session A should keep independent count");
  assert_test(session_mgr.get_sequence(sid_b).size() == 5,
              "Session B should keep independent count");

  std::cout << "Integration Scenario 4 (Concurrent Sessions) - Passed!\n";
}

void test_session_persistence_snapshot() {
  PolicyGraph graph;
  graph.add_node({"safe_tool", "safe_tool", RiskLevel::LOW, NodeType::NORMAL,
                  {}, "", {}});
  graph.add_edge({"safe_tool", "safe_tool", {}, {}});

  PolicyValidator validator(graph);
  SessionManager session_mgr;
  auto mock_factory = [](const std::string &, const SandboxConfig &) {
    return std::make_unique<MockRuntime>();
  };
  SandboxManager sandbox_mgr("", mock_factory);
  sandbox_mgr.load_module("safe_tool", "safe_tool.wasm");

  ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);
  std::string sid = session_mgr.create_session();

  auto first = interceptor.intercept(sid, "safe_tool", {{"step", "1"}});
  auto second = interceptor.intercept(sid, "safe_tool", {{"step", "2"}});
  assert_test(first.first.approved && second.first.approved,
              "Persistence scenario should capture multiple approved calls");

  fs::path out_path = fs::temp_directory_path() / "guardian_integration_session.json";
  session_mgr.persist_session(sid, out_path.string());

  {
    std::ifstream ifs(out_path);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());

    assert_test(content.find("action_sequence") != std::string::npos,
                "Persisted file must include action sequence");
    assert_test(content.find("safe_tool") != std::string::npos,
                "Persisted file must include tool entries for later recovery");
  }

  fs::remove(out_path);
  std::cout << "Integration Scenario 5 (Session Persistence Snapshot) - Passed!\n";
}

void test_error_handling_and_fail_safe() {
  PolicyGraph graph;
  graph.add_node({"safe_tool", "safe_tool", RiskLevel::LOW, NodeType::NORMAL,
                  {}, "", {}});

  PolicyValidator validator(graph);
  SessionManager session_mgr;

  auto fail_factory = [](const std::string &wasm_path, const SandboxConfig &) {
    auto runtime = std::make_unique<MockRuntime>();
    if (wasm_path.find("safe_tool_fail") != std::string::npos) {
      SandboxResult failure;
      failure.success = false;
      failure.error = "internal runtime error";
      runtime->set_next_result(failure);
    }
    return runtime;
  };

  SandboxManager sandbox_mgr("", fail_factory);
  sandbox_mgr.load_module("safe_tool", "safe_tool_fail.wasm");
  ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);

  std::ifstream missing_policy("definitely_missing_policy.json");
  assert_test(!missing_policy.good(), "Missing policy file should be detectable");

  bool invalid_format_caught = false;
  try {
    (void)PolicyGraph::from_json("{not-valid-json}");
  } catch (const std::invalid_argument &) {
    invalid_format_caught = true;
  }
  assert_test(invalid_format_caught,
              "Invalid policy format should be rejected");

  auto [val_err, sb_err] = interceptor.intercept("invalid-session-uuid", "safe_tool", {});
  assert_test(!val_err.approved,
              "Invalid session ID should be completely rejected");
  assert_test(val_err.reason.find("Invalid session") != std::string::npos,
              "Reason should explicitly state invalid session");

  std::string sid = session_mgr.create_session();
  auto [val_fail_safe, sb_fail_safe] = interceptor.intercept(sid, "safe_tool", {});
  assert_test(!val_fail_safe.approved,
              "Internal sandbox error should fail closed (reject)");
  assert_test(sb_fail_safe.has_value() && !sb_fail_safe->success,
              "Sandbox failure details should be surfaced");

  assert_test(session_mgr.get_sequence(sid).empty(),
              "Failed execution must not be appended to session history");

  std::cout << "Integration Scenario 6 (Error Handling + Fail-Safe) - Passed!\n";
}

int main() {
  std::cout << "Starting standalone integration tests...\n";
  test_financial_scenario();
  test_dos_scenario();
  test_developer_scenario();
  test_concurrent_sessions();
  test_session_persistence_snapshot();
  test_error_handling_and_fail_safe();
  std::cout << "All integration tests passed successfully!\n";
  return 0;
}
