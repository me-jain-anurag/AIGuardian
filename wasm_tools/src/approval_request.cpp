// wasm_tools/src/approval_request.cpp
#include <iostream>
#include <string>

int main() {
    std::string input;
    std::getline(std::cin, input);

    // Simulates an async pause for external approval
    std::cout << R"({"status": "pending", "message": "waiting for human approval"})";
    return 0;
}
