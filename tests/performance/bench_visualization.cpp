#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "guardian/visualization.hpp"
#include "guardian/policy_graph.hpp"
#include "guardian/types.hpp"

using namespace guardian;

TEST_CASE("VisualizationEngine Performance Benchmarks", "[performance][visualization]") {
    VisualizationEngine engine;
    VisualizationOptions opts;
    opts.output_format = VisualizationOptions::ASCII;

    auto create_large_graph = [](int node_count) {
        PolicyGraph graph;
        for (int i = 0; i < node_count; ++i) {
            PolicyNode node;
            node.id = "node_" + std::to_string(i);
            node.tool_name = "tool_" + std::to_string(i);
            node.risk_level = (i % 3 == 0) ? RiskLevel::HIGH : RiskLevel::LOW;
            node.node_type = (i % 5 == 0) ? NodeType::SENSITIVE_SOURCE : NodeType::NORMAL;
            graph.add_node(node);
        }
        for (int i = 0; i < node_count - 1; ++i) {
            PolicyEdge edge;
            edge.from_node_id = "node_" + std::to_string(i);
            edge.to_node_id = "node_" + std::to_string(i + 1);
            graph.add_edge(edge);
        }
        return graph;
    };

    SECTION("Small Graph (10 nodes)") {
        auto graph = create_large_graph(10);
        BENCHMARK("render_graph (ASCII) - 10 nodes") {
            return engine.render_graph(graph, opts);
        };
    }

    SECTION("Medium Graph (50 nodes)") {
        auto graph = create_large_graph(50);
        BENCHMARK("render_graph (ASCII) - 50 nodes") {
            return engine.render_graph(graph, opts);
        };
    }

    SECTION("Large Graph (100 nodes)") {
        auto graph = create_large_graph(100);
        BENCHMARK("render_graph (ASCII) - 100 nodes") {
            return engine.render_graph(graph, opts);
        };
    }

    SECTION("Sequence Rendering (10 steps)") {
        auto graph = create_large_graph(20);
        std::vector<ToolCall> sequence;
        std::vector<ValidationResult> results;
        for (int i = 0; i < 10; ++i) {
            ToolCall call;
            call.tool_name = "tool_" + std::to_string(i);
            sequence.push_back(call);
            results.push_back({true, "Allowed", {}, {}, {}});
        }
        BENCHMARK("render_sequence (ASCII) - 10 steps") {
            return engine.render_sequence(graph, sequence, results, opts);
        };
    }
}
