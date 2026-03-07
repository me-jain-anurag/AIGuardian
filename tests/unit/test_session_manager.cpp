// tests/unit/test_session_manager.cpp
// Owner: Dev C
// Unit + property tests for SessionManager
#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>

#include "guardian/session_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace guardian;

// ============================================================================
// Helper: create a ToolCall with given name
// ============================================================================
static ToolCall make_call(const std::string& name,
                          const std::string& sid = "") {
    ToolCall tc;
    tc.tool_name = name;
    tc.timestamp = std::chrono::system_clock::now();
    tc.session_id = sid;
    return tc;
}

// ============================================================================
// Task 3.1 — Session creation and ID uniqueness
// ============================================================================
TEST_CASE("SessionManager: create_session returns unique IDs",
          "[session][creation]") {
    SessionManager mgr;
    std::set<std::string> ids;

    for (int i = 0; i < 100; ++i) {
        auto id = mgr.create_session();
        REQUIRE_FALSE(id.empty());
        REQUIRE(ids.find(id) == ids.end());  // unique
        ids.insert(id);
    }
    REQUIRE(ids.size() == 100);
}

TEST_CASE("SessionManager: new session is findable", "[session][creation]") {
    SessionManager mgr;
    auto id = mgr.create_session();

    REQUIRE(mgr.has_session(id));
    REQUIRE(mgr.get_sequence(id).empty());
}

TEST_CASE("SessionManager: end_session removes it", "[session][lifecycle]") {
    SessionManager mgr;
    auto id = mgr.create_session();

    mgr.end_session(id);
    REQUIRE_FALSE(mgr.has_session(id));
}

TEST_CASE("SessionManager: end_session throws for unknown ID",
          "[session][lifecycle]") {
    SessionManager mgr;
    REQUIRE_THROWS_AS(mgr.end_session("nonexistent"), std::runtime_error);
}

// ============================================================================
// Task 3.3 — Action sequence growth and retrieval
// ============================================================================
TEST_CASE("SessionManager: append and retrieve action sequence",
          "[session][sequence]") {
    SessionManager mgr;
    auto id = mgr.create_session();

    mgr.append_tool_call(id, make_call("read_file", id));
    mgr.append_tool_call(id, make_call("process_data", id));
    mgr.append_tool_call(id, make_call("send_email", id));

    auto seq = mgr.get_sequence(id);
    REQUIRE(seq.size() == 3);
    REQUIRE(seq[0].tool_name == "read_file");
    REQUIRE(seq[1].tool_name == "process_data");
    REQUIRE(seq[2].tool_name == "send_email");
}

TEST_CASE("SessionManager: append_tool_call throws for unknown session",
          "[session][sequence]") {
    SessionManager mgr;
    REQUIRE_THROWS_AS(mgr.append_tool_call("bad_id", make_call("x")),
                      std::runtime_error);
}

// ============================================================================
// Task 3.4 — Session persistence to JSON
// ============================================================================
TEST_CASE("SessionManager: persist_session writes valid JSON",
          "[session][persistence]") {
    SessionManager mgr;
    auto id = mgr.create_session();
    mgr.append_tool_call(id, make_call("tool_a", id));
    mgr.append_tool_call(id, make_call("tool_b", id));

    fs::path tmp = fs::temp_directory_path() / "guardian_test_session.json";
    mgr.persist_session(id, tmp.string());

    // Verify file exists and contains valid JSON
    REQUIRE(fs::exists(tmp));

    {   // Scoped block so ifstream closes before fs::remove (Windows file locking)
        std::ifstream ifs(tmp);
        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        REQUIRE_FALSE(content.empty());

        // Verify key fields
        REQUIRE(content.find("session_id") != std::string::npos);
        REQUIRE(content.find("action_sequence") != std::string::npos);
        REQUIRE(content.find("tool_a") != std::string::npos);
        REQUIRE(content.find("tool_b") != std::string::npos);
    }

    // Cleanup
    fs::remove(tmp);
}

TEST_CASE("SessionManager: persist_session throws for unknown session",
          "[session][persistence]") {
    SessionManager mgr;
    REQUIRE_THROWS_AS(mgr.persist_session("bad_id", "/tmp/x.json"),
                      std::runtime_error);
}

// ============================================================================
// Task 3.5 — Session timeout behavior
// ============================================================================
TEST_CASE("SessionManager: expired session is not visible",
          "[session][timeout]") {
    // 1-second timeout
    SessionManager mgr(1, 0);
    auto id = mgr.create_session();

    REQUIRE(mgr.has_session(id));

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::seconds(2));

    REQUIRE_FALSE(mgr.has_session(id));
}

TEST_CASE("SessionManager: append to expired session throws",
          "[session][timeout]") {
    SessionManager mgr(1, 0);
    auto id = mgr.create_session();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    REQUIRE_THROWS_AS(mgr.append_tool_call(id, make_call("x")),
                      std::runtime_error);
}

// ============================================================================
// Max concurrent sessions
// ============================================================================
TEST_CASE("SessionManager: max sessions enforced", "[session][limits]") {
    SessionManager mgr(0, 3);  // no timeout, max 3 sessions

    mgr.create_session();
    mgr.create_session();
    mgr.create_session();

    REQUIRE_THROWS_AS(mgr.create_session(), std::runtime_error);
}

TEST_CASE("SessionManager: ending a session frees a slot", "[session][limits]") {
    SessionManager mgr(0, 2);

    auto id1 = mgr.create_session();
    mgr.create_session();
    REQUIRE_THROWS(mgr.create_session());

    mgr.end_session(id1);
    REQUIRE_NOTHROW(mgr.create_session());  // slot freed
}

// ============================================================================
// Queries
// ============================================================================
TEST_CASE("SessionManager: get_all_sessions returns active sessions",
          "[session][queries]") {
    SessionManager mgr;
    mgr.create_session();
    mgr.create_session();
    mgr.create_session();

    auto all = mgr.get_all_sessions();
    REQUIRE(all.size() == 3);
}

TEST_CASE("SessionManager: active_session_count is correct",
          "[session][queries]") {
    SessionManager mgr;
    auto id = mgr.create_session();
    mgr.create_session();

    REQUIRE(mgr.active_session_count() == 2);

    mgr.end_session(id);
    REQUIRE(mgr.active_session_count() == 1);
}

// ============================================================================
// Task 3.2 — Concurrent session isolation (multi-threaded test)
// ============================================================================
TEST_CASE("SessionManager: concurrent access is thread-safe",
          "[session][concurrency]") {
    SessionManager mgr;
    constexpr int NUM_THREADS = 8;
    constexpr int OPS_PER_THREAD = 50;

    std::vector<std::thread> threads;
    std::vector<std::string> all_ids(NUM_THREADS);

    // Create sessions concurrently
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&mgr, &all_ids, t]() {
            all_ids[t] = mgr.create_session();
        });
    }
    for (auto& th : threads) th.join();
    threads.clear();

    // Verify all sessions created with unique IDs
    std::set<std::string> id_set(all_ids.begin(), all_ids.end());
    REQUIRE(id_set.size() == NUM_THREADS);

    // Append tool calls concurrently to different sessions
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&mgr, &all_ids, t, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                mgr.append_tool_call(
                    all_ids[t],
                    make_call("tool_" + std::to_string(i), all_ids[t]));
            }
        });
    }
    for (auto& th : threads) th.join();
    threads.clear();

    // Verify each session has the correct number of tool calls (isolation)
    for (int t = 0; t < NUM_THREADS; ++t) {
        auto seq = mgr.get_sequence(all_ids[t]);
        REQUIRE(seq.size() == OPS_PER_THREAD);
    }
}

// ============================================================================
// Task 3.6 — Property tests
// ============================================================================
TEST_CASE("Property: created sessions always have unique IDs",
          "[session][property]") {
    rc::check("unique IDs", []() {
        SessionManager mgr;
        int n = *rc::gen::inRange(2, 20);
        std::set<std::string> ids;
        for (int i = 0; i < n; ++i) {
            ids.insert(mgr.create_session());
        }
        RC_ASSERT(static_cast<int>(ids.size()) == n);
    });
}

TEST_CASE("Property: new sessions start with empty sequence",
          "[session][property]") {
    rc::check("empty initial sequence", []() {
        SessionManager mgr;
        auto id = mgr.create_session();
        RC_ASSERT(mgr.get_sequence(id).empty());
    });
}

TEST_CASE("Property: sequence grows by 1 on each append",
          "[session][property]") {
    rc::check("sequence growth", []() {
        SessionManager mgr;
        auto id = mgr.create_session();
        int n = *rc::gen::inRange(1, 30);
        for (int i = 0; i < n; ++i) {
            mgr.append_tool_call(id, make_call("t" + std::to_string(i), id));
        }
        RC_ASSERT(static_cast<int>(mgr.get_sequence(id).size()) == n);
    });
}

TEST_CASE("Property: sessions are isolated from each other",
          "[session][property]") {
    rc::check("session isolation", []() {
        SessionManager mgr;
        auto id1 = mgr.create_session();
        auto id2 = mgr.create_session();

        int n1 = *rc::gen::inRange(0, 10);
        int n2 = *rc::gen::inRange(0, 10);

        for (int i = 0; i < n1; ++i)
            mgr.append_tool_call(id1, make_call("a", id1));
        for (int i = 0; i < n2; ++i)
            mgr.append_tool_call(id2, make_call("b", id2));

        RC_ASSERT(static_cast<int>(mgr.get_sequence(id1).size()) == n1);
        RC_ASSERT(static_cast<int>(mgr.get_sequence(id2).size()) == n2);
    });
}
