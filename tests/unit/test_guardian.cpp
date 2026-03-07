// tests/unit/test_guardian.cpp
// Owner: Dev A
// Unit tests for Guardian API class
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "guardian/guardian.hpp"
#include "guardian/policy_graph.hpp"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace guardian;

// Helper: write a temp policy file and return path
static std::string write_temp_policy() {
    auto temp = fs::temp_directory_path() / "test_policy.json";

    PolicyGraph graph;

    PolicyNode n1;
    n1.id = "read_accounts";
    n1.tool_name = "read_accounts";
    n1.node_type = NodeType::SENSITIVE_SOURCE;
    n1.risk_level = RiskLevel::HIGH;
    graph.add_node(n1);

    PolicyNode n2;
    n2.id = "generate_report";
    n2.tool_name = "generate_report";
    n2.node_type = NodeType::DATA_PROCESSOR;
    graph.add_node(n2);

    PolicyNode n3;
    n3.id = "send_email";
    n3.tool_name = "send_email";
    n3.node_type = NodeType::EXTERNAL_DESTINATION;
    graph.add_node(n3);

    PolicyEdge e1;
    e1.from_node_id = "read_accounts";
    e1.to_node_id = "generate_report";
    graph.add_edge(e1);

    PolicyEdge e2;
    e2.from_node_id = "generate_report";
    e2.to_node_id = "send_email";
    graph.add_edge(e2);

    std::ofstream file(temp);
    file << graph.to_json();
    file.close();

    return temp.string();
}

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_CASE("Guardian initializes with valid policy file", "[guardian][init]") {
    auto path = write_temp_policy();
    REQUIRE_NOTHROW(Guardian(path));

    Guardian g(path);
    REQUIRE(g.is_initialized());
    REQUIRE(g.get_policy_graph().node_count() == 3);
    REQUIRE(g.get_policy_graph().edge_count() == 2);

    fs::remove(path);
}

TEST_CASE("Guardian throws on missing policy file", "[guardian][init][error]") {
    REQUIRE_THROWS_AS(Guardian("nonexistent_policy.json"), std::runtime_error);
}

TEST_CASE("Guardian throws on invalid policy JSON", "[guardian][init][error]") {
    auto temp = fs::temp_directory_path() / "bad_policy.json";
    std::ofstream file(temp);
    file << "{not valid json}";
    file.close();

    REQUIRE_THROWS_AS(Guardian(temp.string()), std::runtime_error);
    fs::remove(temp);
}

// ============================================================================
// Session Management Tests
// ============================================================================

TEST_CASE("Guardian creates and manages sessions", "[guardian][session]") {
    auto path = write_temp_policy();
    Guardian g(path);

    auto sid = g.create_session();
    REQUIRE_FALSE(sid.empty());

    // End session should not throw
    REQUIRE_NOTHROW(g.end_session(sid));

    fs::remove(path);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_CASE("Guardian validate_tool_call with valid tool", "[guardian][validate]") {
    auto path = write_temp_policy();
    Guardian g(path);
    auto sid = g.create_session();

    auto result = g.validate_tool_call("read_accounts", sid);
    // Currently always approves (validator not linked)
    REQUIRE(result.approved == true);

    g.end_session(sid);
    fs::remove(path);
}

TEST_CASE("Guardian validate_tool_call with unknown tool", "[guardian][validate]") {
    auto path = write_temp_policy();
    Guardian g(path);
    auto sid = g.create_session();

    auto result = g.validate_tool_call("unknown_tool", sid);
    REQUIRE(result.approved == false);
    REQUIRE(result.reason.find("Unknown tool") != std::string::npos);
    REQUIRE_FALSE(result.suggested_alternatives.empty());

    g.end_session(sid);
    fs::remove(path);
}

TEST_CASE("Guardian validate_tool_call with bad session", "[guardian][validate]") {
    auto path = write_temp_policy();
    Guardian g(path);

    auto result = g.validate_tool_call("read_accounts", "bad_session_id");
    REQUIRE(result.approved == false);
    REQUIRE(result.reason.find("Invalid session") != std::string::npos);

    fs::remove(path);
}

// ============================================================================
// Execute Tool Tests
// ============================================================================

TEST_CASE("Guardian execute_tool with valid call", "[guardian][execute]") {
    auto path = write_temp_policy();
    Guardian g(path);
    auto sid = g.create_session();

    auto [validation, sandbox] = g.execute_tool(
        "read_accounts", {{"account", "12345"}}, sid);

    REQUIRE(validation.approved == true);
    // sandbox is nullopt since SandboxManager not linked
    REQUIRE_FALSE(sandbox.has_value());

    g.end_session(sid);
    fs::remove(path);
}

// ============================================================================
// Visualization Tests
// ============================================================================

TEST_CASE("Guardian visualize_policy as DOT", "[guardian][viz]") {
    auto path = write_temp_policy();
    Guardian g(path);

    auto dot = g.visualize_policy("dot");
    REQUIRE(dot.find("digraph") != std::string::npos);
    REQUIRE(dot.find("read_accounts") != std::string::npos);

    fs::remove(path);
}

TEST_CASE("Guardian visualize_policy as JSON", "[guardian][viz]") {
    auto path = write_temp_policy();
    Guardian g(path);

    auto json = g.visualize_policy("json");
    REQUIRE(json.find("nodes") != std::string::npos);

    fs::remove(path);
}

// ============================================================================
// Config Tests
// ============================================================================

TEST_CASE("Guardian config round-trip", "[guardian][config]") {
    auto path = write_temp_policy();
    Guardian g(path);

    Config cfg;
    cfg.sandbox.memory_limit_mb = 256;
    cfg.logging.level = "DEBUG";
    g.update_config(cfg);

    auto retrieved = g.get_config();
    REQUIRE(retrieved.sandbox.memory_limit_mb == 256);
    REQUIRE(retrieved.logging.level == "DEBUG");

    fs::remove(path);
}

// ============================================================================
// Property Tests (RapidCheck)
// ============================================================================

#include <rapidcheck.h>

TEST_CASE("Guardian Property Tests: initialization robustness", "[guardian][property]") {
    // 7.6 Property tests: initialization, result structure, error messages
    rc::check("Guardian throws std::runtime_error on non-existent policy paths",
        [](const std::string& random_path) {
            // Filter out paths that might actually exist or throw different exceptions
            // due to null bytes or filesystem limits
            if (random_path.empty() || random_path.find('\0') != std::string::npos ||
                random_path.size() > 200) {
                return;
            }
            if (fs::exists(random_path)) return;

            try {
                Guardian g(random_path);
                RC_FAIL("Should have thrown");
            } catch (const std::runtime_error& e) {
                RC_ASSERT(std::string(e.what()).find("Guardian: policy file not found") != std::string::npos);
            }
        });

    rc::check("ValidationResult structure always valid for unknown tools",
        [](const std::string& random_tool) {
            auto path = write_temp_policy();
            Guardian g(path);
            auto sid = g.create_session();

            // We know our temp graph has "read_accounts", "generate_report", "send_email"
            if (random_tool == "read_accounts" || random_tool == "generate_report" ||
                random_tool == "send_email") {
                fs::remove(path);
                return;
            }

            auto result = g.validate_tool_call(random_tool, sid);
            RC_ASSERT(!result.approved);
            RC_ASSERT(result.reason.find("Unknown tool") != std::string::npos);
            // Must provide alternatives (since graph is not empty)
            RC_ASSERT(!result.suggested_alternatives.empty());

            g.end_session(sid);
            fs::remove(path);
        });
}
