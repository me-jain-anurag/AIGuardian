// wasm_tools/src/infinite_loop_tool.cpp
#include <iostream>
#include <string>

// This tool intentionally hangs to test watchdog timeouts.
int main() {
    std::string input;
    std::getline(std::cin, input);

    // Used to test the SandboxManager's timeout enforcement
    volatile int counter = 0;
    while (true) {
        counter++;
    }

    return 0;
}
