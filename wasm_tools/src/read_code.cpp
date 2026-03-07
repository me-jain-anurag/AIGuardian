// wasm_tools/src/read_code.cpp
#include <iostream>
#include <fstream>
#include <string>

// Simple JSON extraction for typical input like {"path":"./CMakeLists.txt"}
std::string extract_path(const std::string& json) {
    size_t pos = json.find("\"path\"");
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos + 6);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos + 1); // start of value
    if (pos == std::string::npos) return "";
    size_t end = json.find("\"", pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

int main() {
    std::string input;
    std::getline(std::cin, input);

    std::string path = extract_path(input);
    if (path.empty()) {
        std::cerr << "Missing path parameter\n";
        return 1;
    }

    // Try to open the file to test WASI file access restrictions
    std::ifstream file(path);
    if (!file.is_open()) {
        // If the sandbox blocks it, we won't be able to open it.
        // Returning a non-zero exit code triggers an error in SandboxManager
        std::cerr << "Access denied to path: " << path << "\n";
        return 1; // Error code triggers SandboxViolation or execution failure
    }

    std::cout << R"({"status": "success", "message": "file read successfully"})";
    return 0;
}
