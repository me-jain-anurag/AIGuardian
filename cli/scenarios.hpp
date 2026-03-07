// cli/scenarios.hpp
// Owner: Dev D
// Pre-defined demo scenarios for AIGuardian interactive CLI
#pragma once

#include "guardian/guardian.hpp"
#include <string>
#include <vector>
#include <map>

namespace guardian {
namespace cli {

struct DemoScenario {
    std::string id;
    std::string name;
    std::string description;
    
    // Executes the scenario against the provided Guardian instance
    // Returns true if the final demo goal was met
    bool (*execute)(Guardian& guardian, const std::string& session_id);
};

class ScenarioRegistry {
public:
    static std::vector<DemoScenario> get_all_scenarios();
    static const DemoScenario* get_scenario(const std::string& id);

private:
    static bool run_financial_exfiltration(Guardian& guardian, const std::string& session_id);
    static bool run_dev_workflow(Guardian& guardian, const std::string& session_id);
    static bool run_dos_attack(Guardian& guardian, const std::string& session_id);
};

} // namespace cli
} // namespace guardian
