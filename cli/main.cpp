// cli/main.cpp
// Owner: Dev D
// AIGuardian CLI Interactive Demo Tool
#include "guardian/guardian.hpp"
#include "guardian/logger.hpp"
#include "terminal_ui.hpp"
#include "scenarios.hpp"
#include <iostream>
#include <string>

using namespace guardian;
using namespace guardian::cli;

static void print_usage() {
    std::cout << "Usage: guardian_cli [OPTIONS]\n"
              << "Options:\n"
              << "  --interactive       Run the interactive menu (default)\n"
              << "  --scenario <id>     Run a specific scenario directly and exit\n"
              << "  --policy-path <p>   Path to the policy graph JSON (default: tests/fixtures/demo_policy.json)\n"
              << "  --help              Show this message\n";
}

int main(int argc, char** argv) {
    TerminalUI::init();

    std::string policy_path = "tests/fixtures/demo_policy.json";
    std::string run_scenario_id = "";
    bool interactive = true;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage();
            return 0;
        } else if (arg == "--interactive") {
            interactive = true;
            run_scenario_id = "";
        } else if (arg == "--scenario" && i + 1 < argc) {
            run_scenario_id = argv[++i];
            interactive = false;
        } else if (arg == "--policy-path" && i + 1 < argc) {
            policy_path = argv[++i];
        } else {
            TerminalUI::print_error("Unknown argument: " + arg);
            print_usage();
            return 1;
        }
    }

    try {
        TerminalUI::clear_screen();
        TerminalUI::print_header("AIGuardian CLI Demo");

        // Disable standard logger output for demo cleanliness
        Logger::instance().set_level(LogLevel::ERROR);

        // Initialize Guardian instance
        std::cout << "Initializing Guardian with policy: " << policy_path << "\n";
        Guardian g(policy_path);
        
        // Disable Dev C cycle detection default of 0 to allow the DoS scenario to run a few cycles
        Config cfg;
        cfg.cycle_detection.default_threshold = 5;
        g.update_config(cfg);

        TerminalUI::print_success("Engine Core Loaded Successfully.");
        std::cout << "\n";

        if (!run_scenario_id.empty()) {
            const DemoScenario* s = ScenarioRegistry::get_scenario(run_scenario_id);
            if (!s) {
                TerminalUI::print_error("Unknown scenario ID: " + run_scenario_id);
                return 1;
            }
            std::string session_id = g.create_session();
            s->execute(g, session_id);
            TerminalUI::print_divider();
            std::cout << "\n" << g.visualize_session(session_id, "ascii") << "\n";
            return 0;
        }

        // Interactive Menu Loop
        auto all_scenarios = ScenarioRegistry::get_all_scenarios();
        std::vector<std::string> options;
        for (const auto& s : all_scenarios) {
            options.push_back(s.name + " - " + s.description);
        }
        options.push_back("Exit");

        while (true) {
            TerminalUI::clear_screen();
            TerminalUI::print_header("AIGuardian Interactive Demo");
            
            int choice = TerminalUI::prompt_menu("Available Scenarios:", options);
            if (choice == options.size()) {
                break; // Exit
            }

            const auto& s = all_scenarios[choice - 1];
            TerminalUI::clear_screen();
            TerminalUI::print_header("Running Scenario: " + s.name);
            std::cout << TerminalUI::wrap_color(s.description, Color::Yellow) << "\n\n";

            std::string session_id = g.create_session();
            bool result = s.execute(g, session_id);

            std::cout << "\n";
            TerminalUI::print_divider();
            std::cout << TerminalUI::wrap_color("Final Action Sequence Graph:", Color::Bold) << "\n\n";
            std::cout << g.visualize_session(session_id, "ascii") << "\n";
            
            if (result) {
                TerminalUI::print_success("Scenario successfully demonstrated AIGuardian's capabilities.");
            }

            TerminalUI::wait_for_keypress();
        }

    } catch (const std::exception& e) {
        TerminalUI::print_error(std::string("Fatal Demo Error: ") + e.what());
        return 1;
    }

    return 0;
}
