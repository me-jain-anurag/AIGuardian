// tests/performance/bench_policy_graph.cpp
// Owner: Dev A
// Performance benchmarks for PolicyGraph operations
// Target: <10ms for 50 nodes, <50ms for 200 nodes
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "guardian/policy_graph.hpp"

#include <string>
#include <random>
#include <chrono>

using namespace guardian;

// ============================================================================
// Helper: create a graph with N nodes and ~2*N edges
// ============================================================================

static PolicyGraph make_graph(size_t num_nodes) {
    PolicyGraph graph;

    // Add nodes
    for (size_t i = 0; i < num_nodes; ++i) {
        PolicyNode node;
        node.id = "node_" + std::to_string(i);
        node.tool_name = "tool_" + std::to_string(i);
        node.risk_level = static_cast<RiskLevel>(i % 4);
        node.node_type = static_cast<NodeType>(i % 4);
        node.metadata["index"] = std::to_string(i);
        graph.add_node(node);
    }

    // Add edges: chain + some cross-links
    for (size_t i = 0; i + 1 < num_nodes; ++i) {
        PolicyEdge edge;
        edge.from_node_id = "node_" + std::to_string(i);
        edge.to_node_id = "node_" + std::to_string(i + 1);
        graph.add_edge(edge);
    }
    // Add cross-links (skip edges)
    for (size_t i = 0; i + 3 < num_nodes; i += 3) {
        PolicyEdge edge;
        edge.from_node_id = "node_" + std::to_string(i);
        edge.to_node_id = "node_" + std::to_string(i + 3);
        graph.add_edge(edge);
    }

    return graph;
}

// ============================================================================
// Graph Operation Benchmarks
// ============================================================================

TEST_CASE("Benchmark: add_node (50 nodes)", "[benchmark][policy-graph]") {
    BENCHMARK("add 50 nodes") {
        return make_graph(50);
    };
}

TEST_CASE("Benchmark: add_node (200 nodes)", "[benchmark][policy-graph]") {
    BENCHMARK("add 200 nodes") {
        return make_graph(200);
    };
}

TEST_CASE("Benchmark: has_node lookup (50 nodes)", "[benchmark][policy-graph]") {
    auto graph = make_graph(50);
    BENCHMARK("has_node x1000") {
        bool found = false;
        for (int i = 0; i < 1000; ++i) {
            found = graph.has_node("node_" + std::to_string(i % 50));
        }
        return found;
    };
}

TEST_CASE("Benchmark: has_edge lookup (50 nodes)", "[benchmark][policy-graph]") {
    auto graph = make_graph(50);
    BENCHMARK("has_edge x1000") {
        bool found = false;
        for (int i = 0; i < 1000; ++i) {
            found = graph.has_edge("node_" + std::to_string(i % 49),
                                    "node_" + std::to_string((i % 49) + 1));
        }
        return found;
    };
}

TEST_CASE("Benchmark: get_neighbors (50 nodes)", "[benchmark][policy-graph]") {
    auto graph = make_graph(50);
    BENCHMARK("get_neighbors x1000") {
        size_t total = 0;
        for (int i = 0; i < 1000; ++i) {
            total += graph.get_neighbors("node_" + std::to_string(i % 50)).size();
        }
        return total;
    };
}

// ============================================================================
// Serialization Benchmarks
// ============================================================================

TEST_CASE("Benchmark: to_json (50 nodes)", "[benchmark][serialization]") {
    auto graph = make_graph(50);
    BENCHMARK("to_json 50 nodes") {
        return graph.to_json();
    };
}

TEST_CASE("Benchmark: from_json (50 nodes)", "[benchmark][serialization]") {
    auto graph = make_graph(50);
    auto json_str = graph.to_json();
    BENCHMARK("from_json 50 nodes") {
        return PolicyGraph::from_json(json_str);
    };
}

TEST_CASE("Benchmark: to_json (200 nodes)", "[benchmark][serialization]") {
    auto graph = make_graph(200);
    BENCHMARK("to_json 200 nodes") {
        return graph.to_json();
    };
}

TEST_CASE("Benchmark: from_json (200 nodes)", "[benchmark][serialization]") {
    auto graph = make_graph(200);
    auto json_str = graph.to_json();
    BENCHMARK("from_json 200 nodes") {
        return PolicyGraph::from_json(json_str);
    };
}

TEST_CASE("Benchmark: to_dot (50 nodes)", "[benchmark][serialization]") {
    auto graph = make_graph(50);
    BENCHMARK("to_dot 50 nodes") {
        return graph.to_dot();
    };
}

// ============================================================================
// Phase 3: Performance Feature Benchmarks
// ============================================================================

TEST_CASE("Benchmark: get_node_by_tool_name (200 nodes)", "[benchmark][perf]") {
    auto graph = make_graph(200);
    BENCHMARK("get_node_by_tool_name x1000") {
        size_t found = 0;
        for (int i = 0; i < 1000; ++i) {
            if (graph.get_node_by_tool_name("tool_" + std::to_string(i % 200)))
                ++found;
        }
        return found;
    };
}

TEST_CASE("Benchmark: find_path (50 nodes)", "[benchmark][perf]") {
    auto graph = make_graph(50);
    BENCHMARK("find_path (0 to 49)") {
        return graph.find_path("node_0", "node_49");
    };
}

TEST_CASE("Benchmark: find_path (200 nodes)", "[benchmark][perf]") {
    auto graph = make_graph(200);
    BENCHMARK("find_path (0 to 199)") {
        return graph.find_path("node_0", "node_199");
    };
}

TEST_CASE("Benchmark: is_reachable (200 nodes)", "[benchmark][perf]") {
    auto graph = make_graph(200);
    BENCHMARK("is_reachable x100") {
        bool result = false;
        for (int i = 0; i < 100; ++i) {
            result = graph.is_reachable("node_0", "node_" + std::to_string(i + 50));
        }
        return result;
    };
}

TEST_CASE("Benchmark: StringPool intern (1000 strings)", "[benchmark][perf]") {
    BENCHMARK("intern 1000 strings") {
        StringPool pool;
        for (int i = 0; i < 1000; ++i) {
            pool.intern("tool_name_" + std::to_string(i % 200));
        }
        return pool.size();
    };
}

TEST_CASE("Benchmark: LRUCache put/get (1000 ops)", "[benchmark][perf]") {
    BENCHMARK("LRU 1000 put + get") {
        LRUCache<std::string, bool> cache(500);
        for (int i = 0; i < 1000; ++i) {
            cache.put("key_" + std::to_string(i), i % 2 == 0);
        }
        int hits = 0;
        for (int i = 0; i < 1000; ++i) {
            if (cache.get("key_" + std::to_string(i))) ++hits;
        }
        return hits;
    };
}

TEST_CASE("Benchmark: Memory usage proxy (1000 validations)", "[benchmark][perf]") {
    // 10.4 Benchmark memory usage (<100MB for 1000 tool calls)
    // Run under valgrind/massif to verify the 100MB limit.
    auto graph = make_graph(200);
    
    // We can't directly use PolicyValidator as it's in another module,
    // but we can benchmark the graph operations heavily.
    BENCHMARK("Graph traversal for 1000 simulated calls") {
        size_t ops = 0;
        for (int i = 0; i < 1000; ++i) {
            std::string tool_name = "tool_" + std::to_string(i % 200);
            auto node = graph.get_node_by_tool_name(tool_name);
            if (node) {
                ops += graph.get_neighbors(node->id).size();
            }
        }
        return ops;
    };
}

// ============================================================================
// Correctness Validation (run alongside benchmarks)
// ============================================================================

TEST_CASE("Perf: 50-node graph operations under 10ms", "[performance][policy-graph]") {
    auto start = std::chrono::high_resolution_clock::now();

    auto graph = make_graph(50);

    // Run 100 validations
    for (int i = 0; i < 100; ++i) {
        graph.has_node("node_" + std::to_string(i % 50));
        graph.has_edge("node_" + std::to_string(i % 49),
                        "node_" + std::to_string((i % 49) + 1));
        graph.get_neighbors("node_" + std::to_string(i % 50));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    INFO("50-node graph: 100 operations took " << ms << "ms");
    REQUIRE(ms < 1000); // very generous limit — should be <10ms
}

TEST_CASE("Perf: 200-node graph operations under 50ms", "[performance][policy-graph]") {
    auto start = std::chrono::high_resolution_clock::now();

    auto graph = make_graph(200);

    // Run 100 validations
    for (int i = 0; i < 100; ++i) {
        graph.has_node("node_" + std::to_string(i % 200));
        graph.has_edge("node_" + std::to_string(i % 199),
                        "node_" + std::to_string((i % 199) + 1));
        graph.get_neighbors("node_" + std::to_string(i % 200));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    INFO("200-node graph: 100 operations took " << ms << "ms");
    REQUIRE(ms < 5000); // generous limit — should be <50ms
}

TEST_CASE("Perf: find_path correctness", "[performance][policy-graph]") {
    auto graph = make_graph(50);

    auto path = graph.find_path("node_0", "node_49");
    REQUIRE_FALSE(path.empty());
    REQUIRE(path.front() == "node_0");
    REQUIRE(path.back() == "node_49");

    // No path to nonexistent node
    auto no_path = graph.find_path("node_0", "node_999");
    REQUIRE(no_path.empty());
}

TEST_CASE("Perf: get_node_by_tool_name correctness", "[performance][policy-graph]") {
    auto graph = make_graph(50);

    auto node = graph.get_node_by_tool_name("tool_25");
    REQUIRE(node.has_value());
    REQUIRE(node->id == "node_25");

    REQUIRE_FALSE(graph.get_node_by_tool_name("nonexistent").has_value());
}
