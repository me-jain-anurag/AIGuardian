// tests/unit/test_tool_interceptor.cpp
// Owner: Dev C

#include "guardian/policy_graph.hpp"
#include "guardian/policy_validator.hpp"
#include "guardian/sandbox_manager.hpp"
#include "guardian/session_manager.hpp"
#include "guardian/tool_interceptor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>

using namespace guardian;

static PolicyGraph make_interceptor_graph() {
  PolicyGraph graph;

  graph.add_node({"db_read", "db_read", RiskLevel::HIGH,
                  NodeType::SENSITIVE_SOURCE, {}, "", {}});
  graph.add_node({"send_data", "send_data", RiskLevel::HIGH,
                  NodeType::EXTERNAL_DESTINATION, {}, "", {}});
  graph.add_node({"process_data", "process_data", RiskLevel::LOW,
                  NodeType::DATA_PROCESSOR, {}, "", {}});

  graph.add_edge({"db_read", "process_data", {}, {}});
  graph.add_edge({"process_data", "send_data", {}, {}});
  graph.add_edge({"db_read", "send_data", {}, {}});
  return graph;
}

TEST_CASE("ToolInterceptor Unit Tests", "[interceptor]") {
  PolicyGraph graph = make_interceptor_graph();
  PolicyValidator validator(graph);
  SessionManager session_mgr;

  auto mock_factory = [](const std::string &, const SandboxConfig &) {
    return std::make_unique<MockRuntime>();
  };
  SandboxManager sandbox_mgr("", mock_factory);
  sandbox_mgr.load_module("db_read", "db_read.wasm");
  sandbox_mgr.load_module("process_data", "process_data.wasm");
  sandbox_mgr.load_module("send_data", "send_data.wasm");

  std::string session_id = session_mgr.create_session();
  ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);

  SECTION("Capture completeness and sequence growth") {
    auto [val1, sb1] =
        interceptor.intercept(session_id, "db_read", {{"query", "SELECT 1"}});
    REQUIRE(val1.approved);
    REQUIRE(sb1.has_value());
    REQUIRE(sb1->success);

    auto seq = session_mgr.get_sequence(session_id);
    REQUIRE(seq.size() == 1);
    REQUIRE(seq[0].tool_name == "db_read");
    REQUIRE(seq[0].parameters.at("query") == "SELECT 1");

    auto [val2, sb2] = interceptor.intercept(session_id, "process_data", {});
    REQUIRE(val2.approved);
    REQUIRE(sb2.has_value());
    REQUIRE(sb2->success);

    seq = session_mgr.get_sequence(session_id);
    REQUIRE(seq.size() == 2);
  }

  SECTION("Reject validation blocks execution") {
    auto first = interceptor.intercept(session_id, "db_read", {});
    REQUIRE(first.first.approved);

    auto [val, sb] = interceptor.intercept(session_id, "send_data", {});
    REQUIRE_FALSE(val.approved);
    REQUIRE_FALSE(sb.has_value());

    auto seq = session_mgr.get_sequence(session_id);
    REQUIRE(seq.size() == 1);
  }

  SECTION("Sandbox failure triggers fail-safe") {
    auto fail_factory = [](const std::string &wasm_path, const SandboxConfig &) {
      auto runtime = std::make_unique<MockRuntime>();
      if (wasm_path.find("process_data_fail") != std::string::npos) {
        SandboxResult failure;
        failure.success = false;
        failure.error = "forced mock failure";
        runtime->set_next_result(failure);
      }
      return runtime;
    };

    SandboxManager failing_sandbox("", fail_factory);
    failing_sandbox.load_module("db_read", "db_read_ok.wasm");
    failing_sandbox.load_module("process_data", "process_data_fail.wasm");

    SessionManager local_session_mgr;
    auto sid = local_session_mgr.create_session();
    ToolInterceptor failing_interceptor(&validator, &local_session_mgr,
                                        &failing_sandbox);

    auto first = failing_interceptor.intercept(sid, "db_read", {});
    REQUIRE(first.first.approved);

    auto [val, sb] = failing_interceptor.intercept(sid, "process_data", {});
    REQUIRE_FALSE(val.approved);
    REQUIRE(sb.has_value());
    REQUIRE_FALSE(sb->success);
    REQUIRE(val.reason.find("Execution blocked") != std::string::npos);

    auto seq = local_session_mgr.get_sequence(sid);
    REQUIRE(seq.size() == 1);
  }

  SECTION("execute_if_valid wrapper preserves approval path") {
    bool lambda_ran = false;
    auto [val, sb] = interceptor.execute_if_valid(
        session_id, "db_read", {{"mode", "dry-run"}},
        [&]() { lambda_ran = true; });

    REQUIRE(val.approved);
    REQUIRE(lambda_ran);
    REQUIRE_FALSE(sb.has_value());

    auto seq = session_mgr.get_sequence(session_id);
    REQUIRE(seq.size() == 1);
    REQUIRE(seq[0].parameters.at("mode") == "dry-run");
  }
}

TEST_CASE("ToolInterceptor Property Tests", "[interceptor][property]") {
  rc::check("approved interception appends exactly one action", []() {
    PolicyGraph graph;
    graph.add_node({"safe_tool", "safe_tool", RiskLevel::LOW, NodeType::NORMAL,
                    {}, "", {}});
    graph.add_edge({"safe_tool", "safe_tool", {}, {}});

    PolicyValidator validator(graph);
    SessionManager session_mgr;
    auto factory = [](const std::string &, const SandboxConfig &) {
      return std::make_unique<MockRuntime>();
    };
    SandboxManager sandbox_mgr("", factory);
    sandbox_mgr.load_module("safe_tool", "safe_tool.wasm");

    ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);
    auto sid = session_mgr.create_session();

    int count = *rc::gen::inRange(1, 12);
    for (int i = 0; i < count; ++i) {
      auto [val, sb] = interceptor.intercept(
          sid, "safe_tool", {{"index", std::to_string(i)}});
      RC_ASSERT(val.approved);
      RC_ASSERT(sb.has_value());
      RC_ASSERT(sb->success);
    }

    RC_ASSERT(static_cast<int>(session_mgr.get_sequence(sid).size()) == count);
  });

  rc::check("rejected transition does not increase sequence size", []() {
    PolicyGraph graph = make_interceptor_graph();
    PolicyValidator validator(graph);
    SessionManager session_mgr;
    auto factory = [](const std::string &, const SandboxConfig &) {
      return std::make_unique<MockRuntime>();
    };
    SandboxManager sandbox_mgr("", factory);
    sandbox_mgr.load_module("db_read", "db_read.wasm");
    sandbox_mgr.load_module("send_data", "send_data.wasm");

    ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);
    auto sid = session_mgr.create_session();

    auto first = interceptor.intercept(sid, "db_read", {});
    RC_ASSERT(first.first.approved);

    auto before = session_mgr.get_sequence(sid).size();
    auto second = interceptor.intercept(sid, "send_data", {});
    auto after = session_mgr.get_sequence(sid).size();

    RC_ASSERT(!second.first.approved);
    RC_ASSERT(!second.second.has_value());
    RC_ASSERT(before == after);
  });

  rc::check("approved calls preserve input parameters", []() {
    std::string value = *rc::gen::nonEmpty<std::string>();

    PolicyGraph graph;
    graph.add_node({"safe_tool", "safe_tool", RiskLevel::LOW, NodeType::NORMAL,
                    {}, "", {}});
    PolicyValidator validator(graph);
    SessionManager session_mgr;
    auto factory = [](const std::string &, const SandboxConfig &) {
      return std::make_unique<MockRuntime>();
    };
    SandboxManager sandbox_mgr("", factory);
    sandbox_mgr.load_module("safe_tool", "safe_tool.wasm");

    ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);
    auto sid = session_mgr.create_session();

    auto [val, sb] = interceptor.intercept(sid, "safe_tool", {{"payload", value}});
    RC_ASSERT(val.approved);
    RC_ASSERT(sb.has_value());

    auto seq = session_mgr.get_sequence(sid);
    RC_ASSERT(seq.size() == 1);
    RC_ASSERT(seq[0].parameters.at("payload") == value);
  });
}
