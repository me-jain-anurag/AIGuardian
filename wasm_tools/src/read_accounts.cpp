// wasm_tools/src/read_accounts.cpp
#include <iostream>
#include <string>

int main() {
    // Read JSON parameters from stdin.
    std::string input;
    std::getline(std::cin, input);

    // Simulate reading financial accounts.
    // In a real scenario, this might perform secure I/O or query a protected API.
    std::cout << R"({
  "status": "success",
  "data": {
    "accounts": [
      {"id": "A100", "balance": 50000},
      {"id": "A101", "balance": 1500}
    ]
  }
})";
    return 0;
}
