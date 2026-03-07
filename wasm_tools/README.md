# Wasm Tools

This directory contains the WebAssembly tools used by the Guardian AI Sandbox tests and demo policies.

## Building Tools

To compile the C++ source files into `.wasm` binaries, run the provided build script:

```bash
cd wasm_tools
./build.sh
```

### Dependencies
The `build.sh` script requires `wasi-sdk` to compile C++ into standalone WebAssembly binaries with WASI (WebAssembly System Interface) support.
- If `WASI_SDK_PATH` is set, the script will use that toolchain.
- If not set, the script will automatically download the Linux `wasi-sdk` release (v24.0) into `/tmp/wasi-sdk-24.0` and use it.

## Writing a New Tool

All tools must conform to a simple WASI I/O convention:
1. **Input:** Read a JSON string from standard input (`stdin`).
2. **Output:** Write a JSON string to standard output (`stdout`).
3. **Execution Mode:** Tools act as single-execution CLI binaries (no reactor logic needed; just an `int main()` that reads, processes, and exits).

Example `my_tool.cpp`:
```cpp
#include <iostream>
#include <string>

int main() {
    std::string input;
    std::getline(std::cin, input);

    // Process input ...
    
    std::cout << R"({"status": "success", "result": 42})";
    return 0;
}
```

Save your new tool in `wasm_tools/src/my_tool.cpp` and run `./build.sh`. The new binary will be placed at `wasm_tools/my_tool.wasm`.
