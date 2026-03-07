// tests/performance/bench_validator.cpp
// Owner: Dev C
// Performance benchmarks for PolicyValidator + SessionManager
// Targets: <10ms for 50 nodes, <50ms for 200 nodes, >100 val/s
#define CATCH_CONFIG_MAIN
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>


#include "guardian/policy_graph.hpp"
#include "guardian/policy_validator.hpp"
#include "guardian/sandbox_manager.hpp"
#include "guardian/session_manager.hpp"
#include "guardian/tool_interceptor.hpp"


#include <atomic>
#include <chrono>
#include <thread>
#include <vector>


using namespace guardian;

// ============================================================================
// Helper: create a graph with N nodes and chain edges
// ============================================================================
static PolicyGraph make_validator_graph(size_t num_nodes) {
  PolicyGraph graph;
  for (size_t i = 0; i < num_nodes; ++i) {
    PolicyNode node;
    node.id = "node_" + std::to_string(i);
    node.tool_name = "tool_" + std::to_string(i);
    node.risk_level = (i == 0) ? RiskLevel::HIGH : RiskLevel::LOW;
    node.node_type = (i == 0)               ? NodeType::SENSITIVE_SOURCE
                     : (i == num_nodes - 1) ? NodeType::EXTERNAL_DESTINATION
                                            : NodeType::NORMAL;
    graph.add_node(node);
  }
  // Chain edges: 0→1→2→...→N-1
  for (size_t i = 0; i + 1 < num_nodes; ++i) {
    PolicyEdge edge;
    edge.from_node_id = "node_" + std::to_string(i);
    edge.to_node_id = "node_" + std::to_string(i + 1);
    graph.add_edge(edge);
  }
  // Add some cross-edges for complexity
  for (size_t i = 0; i + 2 < num_nodes; i += 3) {
    PolicyEdge edge;
    edge.from_node_id = "node_" + std::to_string(i);
    edge.to_node_id = "node_" + std::to_string(i + 2);
    graph.add_edge(edge);
  }
  return graph;
}

// ============================================================================
// 15.2 Benchmark: Validation Latency
// ============================================================================

TEST_CASE("Benchmark: Validation latency (50 nodes)",
          "[benchmark][validator]") {
  auto graph = make_validator_graph(50);
  PolicyValidator validator(graph);

  std::vector<ToolCall> seq;
  seq.push_back({"tool_0", {}});

  BENCHMARK("validate single transition (50 nodes)") {
    return validator.validate("tool_1", seq);
  };
}

TEST_CASE("Benchmark: Validation latency (200 nodes)",
          "[benchmark][validator]") {
  auto graph = make_validator_graph(200);
  PolicyValidator validator(graph);

  std::vector<ToolCall> seq;
  seq.push_back({"tool_0", {}});

  BENCHMARK("validate single transition (200 nodes)") {
    return validator.validate("tool_1", seq);
  };
}

TEST_CASE("Benchmark: Full chain validation (50 nodes)",
          "[benchmark][validator]") {
  auto graph = make_validator_graph(50);
  PolicyValidator validator(graph);
  validator.set_cycle_threshold(100);

  BENCHMARK("validate 50-step chain") {
    std::vector<ToolCall> seq;
    for (size_t i = 0; i < 50; ++i) {
      std::string tool = "tool_" + std::to_string(i);
      auto result = validator.validate(tool, seq);
      if (result.approved) {
        seq.push_back({tool, {}});
      }
    }
    return seq.size();
  };
}

// ============================================================================
// 15.3 Benchmark: Throughput (>100 val/s)
// ============================================================================

TEST_CASE("Perf: Throughput exceeds 100 validations/second",
          "[performance][validator]") {
  auto graph = make_validator_graph(50);
  PolicyValidator validator(graph);
  validator.set_cycle_threshold(1000);

  auto start = std::chrono::high_resolution_clock::now();

  const int NUM_VALIDATIONS = 1000;
  int approved = 0;
  for (int i = 0; i < NUM_VALIDATIONS; ++i) {
    std::vector<ToolCall> seq;
    seq.push_back({"tool_0", {}});
    auto result = validator.validate("tool_1", seq);
    if (result.approved)
      ++approved;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
  double val_per_sec = (ms > 0) ? (1000.0 * NUM_VALIDATIONS / ms) : 999999;

  INFO("Throughput: " << val_per_sec << " validations/second (" << ms
                      << "ms for " << NUM_VALIDATIONS << ")");
  REQUIRE(val_per_sec > 100.0);
  REQUIRE(approved == NUM_VALIDATIONS);
}

// ============================================================================
// 15.4 Benchmark: Memory (1000 tool calls in session)
// ============================================================================

TEST_CASE("Perf: Memory bounded after 1000 tool calls",
          "[performance][validator]") {
  auto graph = make_validator_graph(10);
  // Allow self-loops for memory test
  for (int i = 0; i < 10; ++i) {
    PolicyEdge edge;
    edge.from_node_id = "node_" + std::to_string(i);
    edge.to_node_id = "node_" + std::to_string(i);
    graph.add_edge(edge);
  }

  PolicyValidator validator(graph);
  validator.set_cycle_threshold(2000);
  SessionManager session_mgr;

  std::string sid = session_mgr.create_session();

  for (int i = 0; i < 1000; ++i) {
    std::string tool = "tool_" + std::to_string(i % 10);
    auto seq = session_mgr.get_sequence(sid);
    auto result = validator.validate(tool, seq);
    if (result.approved) {
      ToolCall call;
      call.tool_name = tool;
      call.parameters = {{"idx", std::to_string(i)}};
      session_mgr.append_tool_call(sid, call);
    }
  }

  auto final_seq = session_mgr.get_sequence(sid);
  INFO("Final sequence length: " << final_seq.size());
  REQUIRE(final_seq.size() <= 1000);
  REQUIRE(final_seq.size() > 0);
}

// ============================================================================
// 15.5 Benchmark: Concurrent session performance (4 threads)
// ============================================================================

TEST_CASE("Perf: Concurrent sessions (4 threads)", "[performance][validator]") {
  auto graph = make_validator_graph(20);
  // Self-loop for repeated calls
  for (int i = 0; i < 20; ++i) {
    PolicyEdge edge;
    edge.from_node_id = "node_" + std::to_string(i);
    edge.to_node_id = "node_" + std::to_string(i);
    graph.add_edge(edge);
  }

  PolicyValidator validator(graph);
  validator.set_cycle_threshold(500);
  SessionManager session_mgr;

  auto mock_factory = [](const std::string &, const SandboxConfig &) {
    return std::make_unique<MockRuntime>();
  };
  SandboxManager sandbox_mgr("", mock_factory);
  for (int i = 0; i < 20; ++i) {
    sandbox_mgr.load_module("tool_" + std::to_string(i),
                            "tool_" + std::to_string(i) + ".wasm");
  }

  ToolInterceptor interceptor(&validator, &session_mgr, &sandbox_mgr);
  std::atomic<int> total_approved{0};

  const int THREADS = 4;
  const int CALLS_PER_THREAD = 100;

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  for (int t = 0; t < THREADS; ++t) {
    std::string sid = session_mgr.create_session();
    threads.emplace_back([&, sid]() {
      for (int i = 0; i < CALLS_PER_THREAD; ++i) {
        std::string tool = "tool_" + std::to_string(i % 20);
        auto [val, sb] = interceptor.intercept(
            sid, tool, {{"thread_idx", std::to_string(i)}});
        if (val.approved)
          ++total_approved;
      }
    });
  }

  for (auto &t : threads)
    t.join();

  auto end = std::chrono::high_resolution_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

  INFO("Concurrent: " << total_approved.load() << " approved across " << THREADS
                      << " threads in " << ms << "ms");
  REQUIRE(total_approved.load() > 0);
  // No deadlocks = test completes
}

// ============================================================================
// Cache hit rate profiling
// ============================================================================

TEST_CASE("Perf: Cache hit rate after warm-up", "[performance][validator]") {
  auto graph = make_validator_graph(10);
  PolicyValidator validator(graph);
  validator.set_cycle_threshold(1000);

  // Warm-up: run same validation pattern multiple times
  for (int round = 0; round < 50; ++round) {
    std::vector<ToolCall> seq;
    seq.push_back({"tool_0", {}});
    validator.validate("tool_1", seq);
  }

  double hit_rate = validator.get_cache_hit_rate();
  INFO("Cache hit rate: " << (hit_rate * 100.0) << "% ("
                          << validator.get_cache_hits() << " hits, "
                          << validator.get_cache_misses() << " misses)");
  // After warm-up with identical queries, hit rate should be high
  REQUIRE(hit_rate > 0.8);
}

// ============================================================================
// Correctness: Latency targets as assertions
// ============================================================================

TEST_CASE("Perf: 50-node validation under 10ms", "[performance][validator]") {
  auto graph = make_validator_graph(50);
  PolicyValidator validator(graph);
  validator.set_cycle_threshold(1000);

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; ++i) {
    std::vector<ToolCall> seq;
    seq.push_back({"tool_0", {}});
    validator.validate("tool_1", seq);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

  INFO("50-node: 100 validations took " << ms << "ms");
  REQUIRE(ms < 1000); // 100 validations under 1s → <10ms each
}

TEST_CASE("Perf: 200-node validation under 50ms", "[performance][validator]") {
  auto graph = make_validator_graph(200);
  PolicyValidator validator(graph);
  validator.set_cycle_threshold(1000);

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; ++i) {
    std::vector<ToolCall> seq;
    seq.push_back({"tool_0", {}});
    validator.validate("tool_1", seq);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

  INFO("200-node: 100 validations took " << ms << "ms");
  REQUIRE(ms < 5000); // 100 validations under 5s → <50ms each
}
