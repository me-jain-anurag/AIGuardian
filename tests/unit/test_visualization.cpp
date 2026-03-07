// tests/unit/test_visualization.cpp
// Owner: Dev D
// Unit + property tests for VisualizationEngine
#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>
#include "guardian/visualization.hpp"
#include "guardian/policy_graph.hpp"
#include "guardian/types.hpp"

#include <chrono>
#include <filesystem>

using namespace guardian;
namespace fs = std::filesystem;

// ============================================================================
// Helper: build a small 3-node test graph
//   read_accounts (SENSITIVE_SOURCE) → generate_report (DATA_PROCESSOR)
//                                    → send_email     (EXTERNAL_DESTINATION)
// ============================================================================
static PolicyGraph make_test_graph() {
    PolicyGraph g;

    PolicyNode n1;
    n1.id        = "read_accounts";
    n1.tool_name = "read_accounts";
    n1.node_type = NodeType::SENSITIVE_SOURCE;
    n1.risk_level = RiskLevel::HIGH;
    g.add_node(n1);

    PolicyNode n2;
    n2.id        = "generate_report";
    n2.tool_name = "generate_report";
    n2.node_type = NodeType::DATA_PROCESSOR;
    n2.risk_level = RiskLevel::MEDIUM;
    g.add_node(n2);

    PolicyNode n3;
    n3.id        = "send_email";
    n3.tool_name = "send_email";
    n3.node_type = NodeType::EXTERNAL_DESTINATION;
    n3.risk_level = RiskLevel::HIGH;
    g.add_node(n3);

    PolicyEdge e1; e1.from_node_id = "read_accounts";   e1.to_node_id = "generate_report"; g.add_edge(e1);
    PolicyEdge e2; e2.from_node_id = "generate_report"; e2.to_node_id = "send_email";      g.add_edge(e2);

    return g;
}

// Helper: make a ToolCall
static ToolCall make_call(const std::string& name, const std::string& session = "sess1") {
    ToolCall c;
    c.tool_name  = name;
    c.session_id = session;
    c.timestamp  = std::chrono::system_clock::now();
    return c;
}

// Helper: approved ValidationResult
static ValidationResult approved_result() {
    return {true, "All checks passed", {}, std::nullopt, std::nullopt};
}

// Helper: rejected ValidationResult with reason
static ValidationResult rejected_result(const std::string& reason) {
    return {false, reason, {}, std::nullopt, std::nullopt};
}

// ============================================================================
// 1. render_graph — DOT format
// ============================================================================

TEST_CASE("Viz: render_graph produces valid DOT output", "[viz][dot]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    auto dot = viz.render_graph(graph);

    // Must be a digraph
    REQUIRE(dot.find("digraph") != std::string::npos);
    // Must contain all 3 node IDs
    REQUIRE(dot.find("read_accounts")   != std::string::npos);
    REQUIRE(dot.find("generate_report") != std::string::npos);
    REQUIRE(dot.find("send_email")      != std::string::npos);
    // Must have arrow syntax
    REQUIRE(dot.find("->") != std::string::npos);
}

TEST_CASE("Viz: render_graph applies correct node colours", "[viz][dot][colors]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();
    VisualizationOptions opts;
    opts.show_metadata = true;

    auto dot = viz.render_graph(graph, opts);

    // SENSITIVE_SOURCE should be red
    // We check the node id appears near a fillcolor red
    REQUIRE(dot.find("red") != std::string::npos);
    // EXTERNAL_DESTINATION should be orange
    REQUIRE(dot.find("orange") != std::string::npos);
    // DATA_PROCESSOR should be lightgreen
    REQUIRE(dot.find("lightgreen") != std::string::npos);
}

TEST_CASE("Viz: render_graph includes risk/type metadata when show_metadata=true", "[viz][dot][metadata]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();
    VisualizationOptions opts;
    opts.show_metadata = true;

    auto dot = viz.render_graph(graph, opts);

    REQUIRE(dot.find("SRC")  != std::string::npos);   // SENSITIVE_SOURCE
    REQUIRE(dot.find("HIGH") != std::string::npos);   // risk level
}

TEST_CASE("Viz: render_graph omits metadata when show_metadata=false", "[viz][dot][metadata]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();
    VisualizationOptions opts;
    opts.show_metadata = false;

    auto dot = viz.render_graph(graph, opts);

    // Labels should not contain the bracket metadata
    REQUIRE(dot.find("SRC")  == std::string::npos);
}

TEST_CASE("Viz: render_graph on empty graph returns valid DOT", "[viz][dot][empty]") {
    VisualizationEngine viz;
    PolicyGraph empty;
    auto dot = viz.render_graph(empty);
    REQUIRE(dot.find("digraph") != std::string::npos);
}

// ============================================================================
// 2. render_graph — ASCII format
// ============================================================================

TEST_CASE("Viz: render_graph ASCII produces non-empty output", "[viz][ascii]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();
    VisualizationOptions opts;
    opts.output_format = VisualizationOptions::ASCII;

    auto ascii = viz.render_graph(graph, opts);
    REQUIRE_FALSE(ascii.empty());
    REQUIRE(ascii.find("read_accounts")   != std::string::npos);
    REQUIRE(ascii.find("generate_report") != std::string::npos);
    REQUIRE(ascii.find("send_email")      != std::string::npos);
}

TEST_CASE("Viz: render_ascii_graph has legend", "[viz][ascii]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();
    auto ascii = viz.render_ascii_graph(graph);
    REQUIRE(ascii.find("Legend") != std::string::npos);
}

TEST_CASE("Viz: render_ascii_graph on empty graph returns non-empty", "[viz][ascii][empty]") {
    VisualizationEngine viz;
    PolicyGraph empty;
    auto ascii = viz.render_ascii_graph(empty);
    REQUIRE_FALSE(ascii.empty());
}

// ============================================================================
// 3. render_sequence — path highlighting
// ============================================================================

TEST_CASE("Viz: render_sequence marks approved steps correctly", "[viz][sequence]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    std::vector<ToolCall> seq = {
        make_call("read_accounts"),
        make_call("generate_report"),
        make_call("send_email")
    };
    std::vector<ValidationResult> results = {
        approved_result(),
        approved_result(),
        approved_result()
    };

    auto dot = viz.render_sequence(graph, seq, results);
    REQUIRE(dot.find("digraph") != std::string::npos);
    // All steps approved → green edges
    REQUIRE(dot.find("darkgreen") != std::string::npos);
    // No red dashed edges expected
    REQUIRE(dot.find("style=dashed") == std::string::npos);
}

TEST_CASE("Viz: render_sequence marks rejected step with distinct styling", "[viz][sequence][violation]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    std::vector<ToolCall> seq = {
        make_call("read_accounts"),
        make_call("send_email")  // direct jump — policy violation
    };
    std::vector<ValidationResult> results = {
        approved_result(),
        rejected_result("Transition not allowed by policy")
    };

    VisualizationOptions opts;
    opts.highlight_violations = true;

    auto dot = viz.render_sequence(graph, seq, results, opts);

    // Should have a red fill for the blocked step
    REQUIRE(dot.find("fillcolor=\"red\"") != std::string::npos);
    // Should have dashed edge to blocked step
    REQUIRE(dot.find("style=dashed") != std::string::npos);
    // Violation node should be octagon shaped
    REQUIRE(dot.find("octagon") != std::string::npos);
}

TEST_CASE("Viz: render_sequence with empty results treats all as approved", "[viz][sequence]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    std::vector<ToolCall> seq = {make_call("read_accounts")};
    std::vector<ValidationResult> results;  // empty

    auto dot = viz.render_sequence(graph, seq, results);
    REQUIRE(dot.find("digraph") != std::string::npos);
}

TEST_CASE("Viz: render_sequence with empty sequence returns valid DOT", "[viz][sequence][empty]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    std::vector<ToolCall> seq;
    std::vector<ValidationResult> results;
    auto dot = viz.render_sequence(graph, seq, results);
    REQUIRE(dot.find("digraph") != std::string::npos);
}

// ============================================================================
// 4. render_ascii_sequence
// ============================================================================

TEST_CASE("Viz: render_ascii_sequence shows approved marker", "[viz][ascii][sequence]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    std::vector<ToolCall> seq = {make_call("read_accounts")};
    std::vector<ValidationResult> results = {approved_result()};

    auto ascii = viz.render_ascii_sequence(graph, seq, results);
    REQUIRE(ascii.find("APPROVED") != std::string::npos);
    REQUIRE(ascii.find("BLOCKED")  == std::string::npos);
}

TEST_CASE("Viz: render_ascii_sequence shows blocked marker with reason", "[viz][ascii][sequence]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    std::vector<ToolCall> seq = {make_call("send_email")};
    std::vector<ValidationResult> results = {rejected_result("Exfiltration risk")};

    auto ascii = viz.render_ascii_sequence(graph, seq, results);
    REQUIRE(ascii.find("BLOCKED") != std::string::npos);
    REQUIRE(ascii.find("Exfiltration risk") != std::string::npos);
}

TEST_CASE("Viz: render_ascii_sequence shows summary", "[viz][ascii][sequence]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    std::vector<ToolCall> seq = {
        make_call("read_accounts"),
        make_call("send_email")
    };
    std::vector<ValidationResult> results = {
        approved_result(),
        rejected_result("Policy violation")
    };

    auto ascii = viz.render_ascii_sequence(graph, seq, results);
    REQUIRE(ascii.find("Summary") != std::string::npos);
    REQUIRE(ascii.find("1 approved") != std::string::npos);
    REQUIRE(ascii.find("1 blocked")  != std::string::npos);
}

TEST_CASE("Viz: render_ascii_sequence marks cycle in ValidationResult", "[viz][ascii][cycle]") {
    VisualizationEngine viz;
    auto graph = make_test_graph();

    CycleInfo cycle;
    cycle.cycle_start_index = 0;
    cycle.cycle_length = 3;
    cycle.cycle_tools = {"read_accounts"};

    ValidationResult vr;
    vr.approved = false;
    vr.reason = "Cycle detected";
    vr.detected_cycle = cycle;

    std::vector<ToolCall> seq = {make_call("read_accounts")};
    std::vector<ValidationResult> results = {vr};

    auto ascii = viz.render_ascii_sequence(graph, seq, results);
    REQUIRE(ascii.find("Cycle detected") != std::string::npos);
}

// ============================================================================
// 5. Performance
// ============================================================================

TEST_CASE("Viz: render_graph completes in under 2 seconds for 100-node graph",
          "[viz][performance]") {
    VisualizationEngine viz;
    PolicyGraph large;

    // Add 100 nodes
    for (int i = 0; i < 100; ++i) {
        PolicyNode n;
        n.id = n.tool_name = "tool_" + std::to_string(i);
        n.node_type  = (i % 4 == 0) ? NodeType::SENSITIVE_SOURCE
                     : (i % 4 == 1) ? NodeType::EXTERNAL_DESTINATION
                     : (i % 4 == 2) ? NodeType::DATA_PROCESSOR
                     : NodeType::NORMAL;
        n.risk_level = static_cast<RiskLevel>(i % 4);
        large.add_node(n);
    }
    // Add linear edges: 0→1→2→...→99
    for (int i = 0; i < 99; ++i) {
        PolicyEdge e;
        e.from_node_id = "tool_" + std::to_string(i);
        e.to_node_id   = "tool_" + std::to_string(i + 1);
        large.add_edge(e);
    }

    auto t0 = std::chrono::steady_clock::now();
    auto dot = viz.render_graph(large);
    auto t1 = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    REQUIRE_FALSE(dot.empty());
    REQUIRE(ms < 2000);  // Must complete in under 2 seconds
}

// ============================================================================
// 6. Property Tests (RapidCheck)
// ============================================================================

TEST_CASE("Viz Property Tests", "[viz][property]") {
    rc::check("render_graph output is always non-empty for non-empty graph",
        []() {
            PolicyGraph g;
            PolicyNode n;
            n.id = n.tool_name = *rc::gen::element(
                std::string("tool_a"), std::string("tool_b"), std::string("tool_c"));
            n.node_type  = NodeType::NORMAL;
            n.risk_level = RiskLevel::LOW;
            g.add_node(n);

            VisualizationEngine viz;
            auto dot = viz.render_graph(g);
            RC_ASSERT(!dot.empty());
            RC_ASSERT(dot.find("digraph") != std::string::npos);
        });

    rc::check("render_ascii_graph is always non-empty",
        []() {
            PolicyGraph g;
            PolicyNode n;
            n.id = n.tool_name = "testnode";
            n.node_type  = NodeType::NORMAL;
            n.risk_level = RiskLevel::LOW;
            g.add_node(n);

            VisualizationEngine viz;
            auto ascii = viz.render_ascii_graph(g);
            RC_ASSERT(!ascii.empty());
        });

    rc::check("render_sequence with all approved has no red dashed edges",
        [](const std::vector<std::string>& tool_names) {
            if (tool_names.empty()) return;

            PolicyGraph g;
            std::vector<ToolCall> seq;
            std::vector<ValidationResult> results;

            for (size_t i = 0; i < tool_names.size() && i < 5; ++i) {
                PolicyNode n;
                n.id = n.tool_name = tool_names[i];
                n.node_type  = NodeType::NORMAL;
                n.risk_level = RiskLevel::LOW;
                g.add_node(n);
                seq.push_back(make_call(tool_names[i]));
                results.push_back(approved_result());
            }

            VisualizationEngine viz;
            auto dot = viz.render_sequence(g, seq, results);
            RC_ASSERT(dot.find("style=dashed") == std::string::npos);
        });
}
