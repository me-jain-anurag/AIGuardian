// wasm_tools/src/memory_hog.cpp
#include <iostream>
#include <string>
#include <vector>

// This tool intentionally allocates massive amounts of RAM to test memory limits.
int main() {
    std::string input;
    std::getline(std::cin, input);

    // Used to test SandboxManager's memory enforcement.
    // Allocate 1MB chunks continuously.
    std::vector<char*> memory_blocks;
    while (true) {
        char* block = new char[1024 * 1024];
        // Force block into memory by writing to it to bypass lazy allocation
        for (int i = 0; i < 1024 * 1024; i += 4096) {
            block[i] = 'X';
        }
        memory_blocks.push_back(block);
    }

    return 0;
}
