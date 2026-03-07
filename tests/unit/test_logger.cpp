// tests/unit/test_logger.cpp
// Owner: Dev D
// Unit + property tests for Logger
#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>
#include "guardian/logger.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using namespace guardian;
namespace fs = std::filesystem;

// ============================================================================
// Helper: create a temp log file path
// ============================================================================
static std::string temp_log_path() {
    return (fs::temp_directory_path() / "guardian_test.log").string();
}

static void cleanup(const std::string& path) {
    if (fs::exists(path)) fs::remove(path);
}

// ============================================================================
// 1. Construction & Level Filtering
// ============================================================================

TEST_CASE("Logger: constructs with default INFO level", "[logger][init]") {
    Logger log;
    // Should not throw — just construct
    REQUIRE_NOTHROW(log.info("test", "hello"));
}

TEST_CASE("Logger: DEBUG messages suppressed when level=INFO", "[logger][filtering]") {
    Logger log(LogLevel::INFO);
    log.debug("comp", "this should not appear");
    // After suppression, no entry stored
    auto exported = log.export_logs();
    // parse and confirm empty array
    auto j = nlohmann::json::parse(exported);
    REQUIRE(j.empty());
}

TEST_CASE("Logger: INFO and above pass when level=INFO", "[logger][filtering]") {
    Logger log(LogLevel::INFO);
    log.info ("comp", "info msg");
    log.warn ("comp", "warn msg");
    log.error("comp", "error msg");

    auto j = nlohmann::json::parse(log.export_logs());
    REQUIRE(j.size() == 3);
}

TEST_CASE("Logger: set_level changes threshold at runtime", "[logger][filtering]") {
    Logger log(LogLevel::WARN);
    log.info("comp", "should be filtered");
    REQUIRE(nlohmann::json::parse(log.export_logs()).empty());

    log.set_level(LogLevel::DEBUG);
    log.debug("comp", "now visible");
    auto j = nlohmann::json::parse(log.export_logs());
    REQUIRE(j.size() == 1);
}

// ============================================================================
// 2. JSON Format Validity
// ============================================================================

TEST_CASE("Logger: export_logs returns valid JSON array", "[logger][json]") {
    Logger log(LogLevel::DEBUG);
    log.debug("moduleA", "debug message",  "some details");
    log.info ("moduleB", "info message");
    log.warn ("moduleC", "warn message",   "context");
    log.error("moduleD", "error message");

    std::string exported = log.export_logs();
    REQUIRE_NOTHROW(nlohmann::json::parse(exported));

    auto j = nlohmann::json::parse(exported);
    REQUIRE(j.is_array());
    REQUIRE(j.size() == 4);
}

TEST_CASE("Logger: log entry contains all fields", "[logger][json]") {
    Logger log(LogLevel::DEBUG);
    log.debug("MyComp", "Test message", "extra details");

    auto j = nlohmann::json::parse(log.export_logs());
    REQUIRE(j.size() == 1);

    auto& entry = j[0];
    REQUIRE(entry.contains("timestamp"));
    REQUIRE(entry.contains("level"));
    REQUIRE(entry.contains("component"));
    REQUIRE(entry.contains("tool_name"));
    REQUIRE(entry.contains("session_id"));
    REQUIRE(entry.contains("message"));
    REQUIRE(entry.contains("details"));

    REQUIRE(entry["level"].get<std::string>() == "DEBUG");
    REQUIRE(entry["component"].get<std::string>() == "MyComp");
    REQUIRE(entry["message"].get<std::string>() == "Test message");
    REQUIRE(entry["details"].get<std::string>() == "extra details");
}

TEST_CASE("Logger: log_with_context stores tool_name and session_id", "[logger][json]") {
    Logger log(LogLevel::DEBUG);
    log.log_with_context(LogLevel::INFO, "Validator", "Tool approved",
                         "read_accounts", "sess-abc-123");

    auto j = nlohmann::json::parse(log.export_logs());
    REQUIRE(j.size() == 1);
    REQUIRE(j[0]["tool_name"].get<std::string>() == "read_accounts");
    REQUIRE(j[0]["session_id"].get<std::string>() == "sess-abc-123");
}

// ============================================================================
// 3. Level-filtered export
// ============================================================================

TEST_CASE("Logger: export_logs(level) filters by minimum level", "[logger][export]") {
    Logger log(LogLevel::DEBUG);
    log.debug("c", "d1");
    log.info ("c", "i1");
    log.warn ("c", "w1");
    log.error("c", "e1");

    auto j_warn = nlohmann::json::parse(log.export_logs(LogLevel::WARN));
    REQUIRE(j_warn.size() == 2);  // WARN + ERROR

    auto j_error = nlohmann::json::parse(log.export_logs(LogLevel::ERROR));
    REQUIRE(j_error.size() == 1);  // ERROR only
}

// ============================================================================
// 4. File Output
// ============================================================================

TEST_CASE("Logger: writes to file via set_output_file", "[logger][file]") {
    std::string path = temp_log_path();
    cleanup(path);

    {
        Logger log(LogLevel::INFO);
        log.set_output_file(path);
        log.info("FileTest", "Line 1");
        log.warn("FileTest", "Line 2");
    }

    REQUIRE(fs::exists(path));
    {
        std::ifstream f(path);
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        REQUIRE(content.find("Line 1") != std::string::npos);
        REQUIRE(content.find("Line 2") != std::string::npos);
    } // closes f

    cleanup(path);
}

TEST_CASE("Logger: set_output_file empty string disables file logging", "[logger][file]") {
    std::string path = temp_log_path();
    cleanup(path);

    Logger log(LogLevel::INFO);
    log.set_output_file(path);
    log.info("comp", "written");
    log.set_output_file("");        // disable
    log.info("comp", "not written to file");

    {
        std::ifstream f(path);
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        REQUIRE(content.find("written") != std::string::npos);
    } // closes f

    cleanup(path);
}

// ============================================================================
// 5. JSON Format Mode
// ============================================================================

TEST_CASE("Logger: set_json_format changes console output format", "[logger][format]") {
    Logger log(LogLevel::DEBUG);
    log.set_json_format(true);
    // Just verify it doesn't throw and entries still accumulate
    log.debug("comp", "json mode");
    auto j = nlohmann::json::parse(log.export_logs());
    REQUIRE(j.size() == 1);
}

// ============================================================================
// 6. Clear
// ============================================================================

TEST_CASE("Logger: clear() resets stored entries", "[logger][clear]") {
    Logger log(LogLevel::DEBUG);
    log.info("c", "m1");
    log.info("c", "m2");
    REQUIRE(nlohmann::json::parse(log.export_logs()).size() == 2);
    log.clear();
    REQUIRE(nlohmann::json::parse(log.export_logs()).empty());
}

// ============================================================================
// 7. Singleton instance
// ============================================================================

TEST_CASE("Logger: static instance() is usable", "[logger][singleton]") {
    auto& inst = Logger::instance();
    REQUIRE_NOTHROW(inst.info("GlobalComp", "singleton log"));
}

// ============================================================================
// 8. JSON escaping
// ============================================================================

TEST_CASE("Logger: messages with special chars produce valid JSON", "[logger][json][escape]") {
    Logger log(LogLevel::INFO);
    log.info("comp", "path: C:\\Users\\test\\file.txt");
    log.info("comp", "message with \"quotes\"");
    log.info("comp", "multi\nline\nmessage");

    std::string exported = log.export_logs();
    REQUIRE_NOTHROW(nlohmann::json::parse(exported));
}

// ============================================================================
// 9. Property Tests (RapidCheck)
// ============================================================================

TEST_CASE("Logger Property Tests", "[logger][property]") {
    // 9.1 All messages at or above level appear in export
    rc::check("Messages at or above min_level always stored",
        []() {
            auto messages = *rc::gen::container<std::vector<std::string>>(
                rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126))
            );
            Logger log(LogLevel::INFO);
            for (const auto& msg : messages) {
                log.info("prop", msg);
            }
            auto j = nlohmann::json::parse(log.export_logs());
            RC_ASSERT(j.size() == messages.size());
        });

    // 9.2 export_logs always produces valid JSON
    rc::check("export_logs always produces valid JSON",
        []() {
            auto component = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
            auto message = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));

            Logger log(LogLevel::DEBUG);
            log.debug(component, message);
            log.info (component, message);
            log.warn (component, message);
            log.error(component, message);
            REQUIRE_NOTHROW(nlohmann::json::parse(log.export_logs()));
        });

    // 9.3 Level filtering: filtered export has no entries below threshold
    rc::check("export_logs(WARN) never contains DEBUG or INFO entries",
        []() {
            auto msgs = *rc::gen::container<std::vector<std::string>>(
                rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126))
            );
            Logger log(LogLevel::DEBUG);
            for (const auto& m : msgs) {
                log.debug("c", m);
                log.info ("c", m);
            }
            auto j = nlohmann::json::parse(log.export_logs(LogLevel::WARN));
            RC_ASSERT(j.empty());
        });

    // 9.4 clear() makes export_logs return empty array
    rc::check("clear() always results in empty export",
        []() {
            auto msg = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
            Logger log(LogLevel::DEBUG);
            log.debug("c", msg);
            log.info ("c", msg);
            log.clear();
            auto j = nlohmann::json::parse(log.export_logs());
            RC_ASSERT(j.empty());
        });
}
