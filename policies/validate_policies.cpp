#include "guardian/policy_graph.hpp"
#include <iostream>
#include <fstream>
#include <vector>

using namespace guardian;

bool validate_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open " << path << std::endl;
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        (std::istreambuf_iterator<char>()));
    try {
        auto graph = PolicyGraph::from_json(content);
        std::cout << "SUCCESS: " << path << " -> " 
                  << graph.node_count() << " nodes, " 
                  << graph.edge_count() << " edges." << std::endl;
        return true;
    } catch(const std::exception& e) {
        std::cerr << "FAILED: " << path << " -> Error: " << e.what() << std::endl;
        return false;
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> files = {
        "policies/financial.json",
        "policies/developer.json",
        "policies/dos_prevention.json"
    };
    bool all_passed = true;
    for (const auto& f : files) {
        if (!validate_file(f)) all_passed = false;
    }
    return all_passed ? 0 : 1;
}
