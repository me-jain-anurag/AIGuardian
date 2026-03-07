// tests/unit/test_policy_validator.cpp
// Owner: Dev C
// Unit + property tests for PolicyValidator
#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>

#include "guardian/policy_validator.hpp"
#include "guardian/config.hpp"

#include <set>

using namespace guardian;

// ============================================================================
// Helper: build a test policy graph
// ============================================================================
//
// Graph topology:
//   file_read (SENSITIVE_SOURCE) -> process_data (DATA_PROCESSOR) -> send_email (EXTERNAL_DESTINATION)
//   file_read -> db_write (NORMAL)
//   process_data -> db_write
//   db_write -> file_read
//
static PolicyGraph make_test_graph() {
    PolicyGraph g;

    PolicyNode file_read;
    file_read.id = "file_read";
    file_read.tool_name = "file_read";
    file_read.node_type = NodeType::SENSITIVE_SOURCE;
    file_read.risk_level = RiskLevel::HIGH;
    g.add_node(file_read);

    PolicyNode process_data;
    process_data.id = "process_data";
    process_data.tool_name = "process_data";
    process_data.node_type = NodeType::DATA_PROCESSOR;
    process_data.risk_level = RiskLevel::LOW;
    g.add_node(process_data);

    PolicyNode send_email;
    send_email.id = "send_email";
    send_email.tool_name = "send_email";
    send_email.node_type = NodeType::EXTERNAL_DESTINATION;
    send_email.risk_level = RiskLevel::HIGH;
    g.add_node(send_email);

    PolicyNode db_write;
    db_write.id = "db_write";
    db_write.tool_name = "db_write";
    db_write.node_type = NodeType::NORMAL;
    db_write.risk_level = RiskLevel::MEDIUM;
    g.add_node(db_write);

    PolicyEdge e1;
    e1.from_node_id = "file_read";
    e1.to_node_id = "process_data";
    g.add_edge(e1);

    PolicyEdge e2;
    e2.from_node_id = "process_data";
    e2.to_node_id = "send_email";
    g.add_edge(e2);

    PolicyEdge e3;
    e3.from_node_id = "file_read";
    e3.to_node_id = "db_write";
    g.add_edge(e3);

    PolicyEdge e4;
    e4.from_node_id = "process_data";
    e4.to_node_id = "db_write";
    g.add_edge(e4);

    PolicyEdge e5;
    e5.from_node_id = "db_write";
    e5.to_node_id = "file_read";
    g.add_edge(e5);

    return g;
}

static ToolCall make_call(const std::string& name) {
    ToolCall tc;
    tc.tool_name = name;
    tc.timestamp = std::chrono::system_clock::now();
    return tc;
}

// ============================================================================
// Task 9.1 — Transition validation
// ============================================================================

TEST_CASE("PolicyValidator: valid transitions are allowed",
          "[validator][transition]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    REQUIRE(validator.check_transition("file_read", "process_data"));
    REQUIRE(validator.check_transition("process_data", "send_email"));
    REQUIRE(validator.check_transition("file_read", "db_write"));
    REQUIRE(validator.check_transition("db_write", "file_read"));
}

TEST_CASE("PolicyValidator: invalid transitions are rejected",
          "[validator][transition]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    REQUIRE_FALSE(validator.check_transition("file_read", "send_email"));
    REQUIRE_FALSE(validator.check_transition("send_email", "file_read"));
    REQUIRE_FALSE(validator.check_transition("db_write", "send_email"));
}

// ============================================================================
// Task 9.2 — Initial tool call (empty sequence)
// ============================================================================

TEST_CASE("PolicyValidator: initial tool call allows any graph node",
          "[validator][transition]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    // Empty from_tool = initial call
    REQUIRE(validator.check_transition("", "file_read"));
    REQUIRE(validator.check_transition("", "process_data"));
    REQUIRE(validator.check_transition("", "send_email"));
    REQUIRE(validator.check_transition("", "db_write"));
}

TEST_CASE("PolicyValidator: initial call rejects unknown tools",
          "[validator][transition]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    REQUIRE_FALSE(validator.check_transition("", "nonexistent_tool"));
}

// ============================================================================
// Task 9.3 — Cycle detection with thresholds
// ============================================================================

TEST_CASE("PolicyValidator: no cycle in short sequence",
          "[validator][cycle]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    std::vector<ToolCall> seq = {make_call("file_read"), make_call("process_data")};
    auto cycle = validator.detect_cycle(seq);
    REQUIRE_FALSE(cycle.has_value());
}

TEST_CASE("PolicyValidator: cycle detected when threshold exceeded",
          "[validator][cycle]") {
    auto graph = make_test_graph();
    Config config;
    config.cycle_detection.default_threshold = 3;
    PolicyValidator validator(graph, config);

    // 4 consecutive calls of same tool exceeds threshold of 3
    std::vector<ToolCall> seq;
    for (int i = 0; i < 4; ++i) {
        seq.push_back(make_call("file_read"));
    }

    auto cycle = validator.detect_cycle(seq);
    REQUIRE(cycle.has_value());
    REQUIRE(cycle->cycle_tools[0] == "file_read");
    REQUIRE(cycle->cycle_length == 4);
}

TEST_CASE("PolicyValidator: per-tool threshold overrides default",
          "[validator][cycle]") {
    auto graph = make_test_graph();
    Config config;
    config.cycle_detection.default_threshold = 10;
    config.cycle_detection.per_tool_thresholds["file_read"] = 2;
    PolicyValidator validator(graph, config);

    // 3 consecutive file_read calls exceeds per-tool threshold of 2
    std::vector<ToolCall> seq = {
        make_call("file_read"),
        make_call("file_read"),
        make_call("file_read"),
    };

    auto cycle = validator.detect_cycle(seq);
    REQUIRE(cycle.has_value());
    REQUIRE(cycle->cycle_tools[0] == "file_read");
}

TEST_CASE("PolicyValidator: non-consecutive repetitions are fine",
          "[validator][cycle]") {
    auto graph = make_test_graph();
    Config config;
    config.cycle_detection.default_threshold = 3;
    PolicyValidator validator(graph, config);

    // file_read appears 4 times but not consecutively
    std::vector<ToolCall> seq = {
        make_call("file_read"),
        make_call("process_data"),
        make_call("file_read"),
        make_call("process_data"),
        make_call("file_read"),
        make_call("process_data"),
        make_call("file_read"),
    };

    auto cycle = validator.detect_cycle(seq);
    REQUIRE_FALSE(cycle.has_value());
}

// ============================================================================
// Task 9.4 — Exfiltration detection
// ============================================================================

TEST_CASE("PolicyValidator: exfiltration detected on direct sensitive->external path",
          "[validator][exfiltration]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    // file_read (SENSITIVE) directly to send_email (EXTERNAL) — no processor in between
    std::vector<ToolCall> seq = {
        make_call("file_read"),
        make_call("send_email"),
    };

    auto exfil = validator.detect_exfiltration(seq);
    REQUIRE(exfil.has_value());
    REQUIRE(exfil->source_node == "file_read");
    REQUIRE(exfil->destination_node == "send_email");
}

TEST_CASE("PolicyValidator: no exfiltration when processor is in between",
          "[validator][exfiltration]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    // file_read -> process_data (DATA_PROCESSOR) -> send_email — safe
    std::vector<ToolCall> seq = {
        make_call("file_read"),
        make_call("process_data"),
        make_call("send_email"),
    };

    auto exfil = validator.detect_exfiltration(seq);
    REQUIRE_FALSE(exfil.has_value());
}

TEST_CASE("PolicyValidator: no exfiltration with only normal tools",
          "[validator][exfiltration]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    std::vector<ToolCall> seq = {
        make_call("db_write"),
        make_call("file_read"),
        make_call("db_write"),
    };

    auto exfil = validator.detect_exfiltration(seq);
    // file_read is SENSITIVE but db_write is NORMAL, not EXTERNAL_DESTINATION
    REQUIRE_FALSE(exfil.has_value());
}

// ============================================================================
// Task 9.5 — Validation caching
// ============================================================================

TEST_CASE("PolicyValidator: cached result matches fresh result",
          "[validator][cache]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    std::vector<ToolCall> seq = {make_call("file_read")};

    // First call — computes
    auto result1 = validator.validate("process_data", seq);
    // Second call — should hit cache
    auto result2 = validator.validate("process_data", seq);

    REQUIRE(result1.approved == result2.approved);
    REQUIRE(result1.reason == result2.reason);
}

// ============================================================================
// Task 9.6 — Deterministic validation
// ============================================================================

TEST_CASE("PolicyValidator: validation is deterministic",
          "[validator][determinism]") {
    auto graph = make_test_graph();
    std::vector<ToolCall> seq = {make_call("file_read")};

    // Create fresh validators for independent runs
    PolicyValidator v1(graph);
    PolicyValidator v2(graph);

    auto r1 = v1.validate("process_data", seq);
    auto r2 = v2.validate("process_data", seq);

    REQUIRE(r1.approved == r2.approved);
    REQUIRE(r1.reason == r2.reason);
}

// ============================================================================
// Path validation
// ============================================================================

TEST_CASE("PolicyValidator: valid path through graph is accepted",
          "[validator][path]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    REQUIRE(validator.is_valid_path({"file_read", "process_data", "send_email"}));
    REQUIRE(validator.is_valid_path({"file_read", "db_write", "file_read"}));
    REQUIRE(validator.is_valid_path({"db_write", "file_read", "process_data", "db_write"}));
}

TEST_CASE("PolicyValidator: invalid path is rejected",
          "[validator][path]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    REQUIRE_FALSE(validator.is_valid_path({"file_read", "send_email"}));
    REQUIRE_FALSE(validator.is_valid_path({"send_email", "file_read"}));
}

TEST_CASE("PolicyValidator: empty and single-node paths",
          "[validator][path]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    REQUIRE(validator.is_valid_path({}));
    REQUIRE(validator.is_valid_path({"file_read"}));
    REQUIRE_FALSE(validator.is_valid_path({"unknown_tool"}));
}

// ============================================================================
// Main validate() integration
// ============================================================================

TEST_CASE("PolicyValidator: validate approves valid initial call",
          "[validator][validate]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    auto result = validator.validate("file_read", {});
    REQUIRE(result.approved);
}

TEST_CASE("PolicyValidator: validate rejects unknown tool",
          "[validator][validate]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    auto result = validator.validate("nonexistent", {});
    REQUIRE_FALSE(result.approved);
    REQUIRE(result.reason.find("not defined") != std::string::npos);
}

TEST_CASE("PolicyValidator: validate rejects invalid transition",
          "[validator][validate]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    std::vector<ToolCall> seq = {make_call("file_read")};
    auto result = validator.validate("send_email", seq);
    REQUIRE_FALSE(result.approved);
    REQUIRE(result.reason.find("not allowed") != std::string::npos);
    REQUIRE_FALSE(result.suggested_alternatives.empty());
}

TEST_CASE("PolicyValidator: validate detects exfiltration attempt",
          "[validator][validate]") {
    auto graph = make_test_graph();

    // Add direct edge from file_read to send_email for this test
    PolicyEdge direct;
    direct.from_node_id = "file_read";
    direct.to_node_id = "send_email";
    graph.add_edge(direct);

    PolicyValidator validator(graph);

    std::vector<ToolCall> seq = {make_call("file_read")};
    auto result = validator.validate("send_email", seq);
    REQUIRE_FALSE(result.approved);
    REQUIRE(result.detected_exfiltration.has_value());
}

TEST_CASE("PolicyValidator: get_alternatives returns valid next tools",
          "[validator][alternatives]") {
    auto graph = make_test_graph();
    PolicyValidator validator(graph);

    auto alts = validator.get_alternatives("file_read");
    REQUIRE(alts.size() == 2);
    std::set<std::string> alt_set(alts.begin(), alts.end());
    REQUIRE(alt_set.count("process_data"));
    REQUIRE(alt_set.count("db_write"));
}

// ============================================================================
// Task 9.7 — Property tests
// ============================================================================

TEST_CASE("Property: check_transition is deterministic",
          "[validator][property]") {
    rc::check("deterministic transitions", []() {
        auto graph = make_test_graph();
        PolicyValidator v1(graph);
        PolicyValidator v2(graph);

        auto tools = graph.get_all_node_ids();
        if (tools.size() < 2) return;

        auto from = tools[*rc::gen::inRange(size_t{0}, tools.size())];
        auto to = tools[*rc::gen::inRange(size_t{0}, tools.size())];

        RC_ASSERT(v1.check_transition(from, to) == v2.check_transition(from, to));
    });
}

TEST_CASE("Property: validate result structure is always complete",
          "[validator][property]") {
    rc::check("result completeness", []() {
        auto graph = make_test_graph();
        PolicyValidator validator(graph);

        auto tools = graph.get_all_node_ids();
        auto tool = tools[*rc::gen::inRange(size_t{0}, tools.size())];

        auto result = validator.validate(tool, {});
        // Every result must have a reason
        RC_ASSERT(!result.reason.empty());
    });
}

TEST_CASE("Property: is_valid_path validates single edges correctly",
          "[validator][property]") {
    rc::check("single edge path == has_edge", []() {
        auto graph = make_test_graph();
        PolicyValidator validator(graph);

        auto tools = graph.get_all_node_ids();
        if (tools.size() < 2) return;

        auto from = tools[*rc::gen::inRange(size_t{0}, tools.size())];
        auto to = tools[*rc::gen::inRange(size_t{0}, tools.size())];

        // A 2-node path validity should match edge existence
        bool path_valid = validator.is_valid_path({from, to});
        bool edge_exists = graph.has_edge(from, to);
        RC_ASSERT(path_valid == edge_exists);
    });
}

TEST_CASE("Property: safe path (with DATA_PROCESSOR) is not flagged as exfiltration",
          "[validator][property]") {
    rc::check("safe path no exfiltration", []() {
        auto graph = make_test_graph();
        PolicyValidator validator(graph);

        // file_read -> process_data -> send_email is always safe
        std::vector<ToolCall> safe_seq = {
            make_call("file_read"),
            make_call("process_data"),
            make_call("send_email"),
        };
        auto exfil = validator.detect_exfiltration(safe_seq);
        RC_ASSERT(!exfil.has_value());
    });
}
