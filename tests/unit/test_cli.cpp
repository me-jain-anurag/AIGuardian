// tests/unit/test_cli.cpp
// Owner: Dev D
// Sanity checks for the CLI Demo Tool scenarios
#include <catch2/catch_test_macros.hpp>
#include "guardian/guardian.hpp"
#include "../../cli/scenarios.hpp"

using namespace guardian;
using namespace guardian::cli;

TEST_CASE("CLI: Scenario 1 (Finance) is blocked", "[cli][scenarios]") {
    Guardian g("tests/fixtures/demo_policy.json");
    auto session = g.create_session();
    // execute() returns TRUE if the scenario reached its expected conclusion
    // For Scenario 1, the expected conclusion is that AIGuardian blocked the network call.
    REQUIRE(ScenarioRegistry::get_scenario("finance")->execute(g, session));
}

TEST_CASE("CLI: Scenario 2 (Dev) is approved", "[cli][scenarios]") {
    Guardian g("tests/fixtures/demo_policy.json");
    auto session = g.create_session();
    // Expected conclusion: all actions are approved
    REQUIRE(ScenarioRegistry::get_scenario("dev")->execute(g, session));
}

TEST_CASE("CLI: Scenario 3 (DoS) is blocked by cycle detection", "[cli][scenarios]") {
    Guardian g("tests/fixtures/demo_policy.json");
    // Ensure threshold is low enough to trigger quickly during tests
    Config cfg;
    cfg.cycle_detection.default_threshold = 5;
    g.update_config(cfg);
    
    auto session = g.create_session();
    // Expected conclusion: infinite loop is halted
    REQUIRE(ScenarioRegistry::get_scenario("dos")->execute(g, session));
}
